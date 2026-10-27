#!/usr/bin/env python3
"""PreToolUse hook: keeps the agent out of locked zones (.agent/zones.json), hard-gates git pushes/merges/history
rewrites and settings files, and logs every tool call.

Runs before EVERY tool call. Reads the hook JSON on stdin, prints a deny decision on stdout when the call would
 - read / search / change a locked zone, or change a read-only zone (harness = the hooks themselves),
 - push, merge, rebase, rewrite history or delete branches/tags (git or gh) - the user runs those with "!",
 - change any Agent settings file or turn hooks off,
 - run a script the agent wrote (.scratch/, /tmp) whose text names a locked zone (a script's own writes are invisible here).
Same input -> same answer. Trace: .agent/logs/trace.jsonl (time, session, tool, target, decision, rule).
Fails closed: if this script breaks, file/shell tools are denied with the error (fix with: ! git checkout .agent/).
Test without the agent:  python3 .agent/hooks/test_zone_guard.py
"""
import glob, json, os, re, shlex, sys, time

HOOKS = os.path.dirname(os.path.abspath(__file__))
AGENT_DIR = os.path.dirname(HOOKS)
ROOT = os.path.dirname(AGENT_DIR)
ZONES = os.path.join(AGENT_DIR, "zones.json")
UNLOCK = os.path.join(AGENT_DIR, "zones.unlock")      # written only by .agent/zone.sh, run by the user
TRACE = os.path.join(AGENT_DIR, "logs", "trace.jsonl")
WIN_ROOT = "c:/path/to/project"   # the project root as Windows writes it (lower case)

FILE_TOOLS = {"Read": "read", "Edit": "edit", "MultiEdit": "edit", "Write": "edit", "NotebookEdit": "edit"}
SEARCH_TOOLS = {"Glob", "Grep"}
GUARDED = set(FILE_TOOLS) | SEARCH_TOOLS | {"Bash"}
SEP = r"&&|\|\||[;|\n]"
GLOB_CH = re.compile(r"[*?\[]")

# ---- hard gates (not unlockable by zones; the user runs these commands with "!") ----
# Only where a COMMAND starts (start / new line / after && ; | ( ` $( ), optionally after VAR=x, sudo, command, exec,
# time or a path) - so docs, notes and commit messages may mention "git push" in plain words.
CMD = r"(?:^|[;&|\n(`]|\$\()\s*(?:\w+=\S*\s+)*(?:(?:sudo|command|exec|time|nohup)\s+)*(?:\S*/)?"
GIT = CMD + r"git(?:\s+-[Cc]\s+(?:\"[^\"]*\"|'[^']*'|\S+))*\s+"   # git + optional -C dir / -c k=v, then the subcommand
GH = CMD + r"gh\s+"
RISKY = r"(push|merge|rebase|pull|filter-branch|filter-repo|cherry-pick|revert)\b"
GIT_GATES = [
    (GIT + RISKY, "push/merge/rebase/pull/history change"),
    (GIT + r"reset\b[^;&|\n]*--hard\b", "reset --hard"),
    (GIT + r"commit\b[^;&|\n]*\s--amend\b", "commit --amend (history rewrite)"),
    (GIT + r"branch\b[^;&|\n]*\s-(d|D|-delete)\b", "branch delete"),
    (GIT + r"tag\b[^;&|\n]*\s-(d|-delete)\b", "tag delete"),
    (GH + r"(repo|release|pr\s+merge|api\b[^;&|\n]*-X\s*(DELETE|PATCH|PUT|POST))", "GitHub repo/release/merge change"),
    # the same, hidden in a string that gets run: bash -c "...", eval "...", subprocess(['git', 'push'])
    (CMD + r"(bash|sh|zsh|eval)\b[^\n]*['\"][^'\"]*\bgit\b[^'\"]*\b" + RISKY, "push/merge/... inside a string that gets run"),
    (r"['\"]git['\"]\s*,\s*(['\"]-C['\"]\s*,\s*['\"][^'\"]*['\"]\s*,\s*)?['\"]" + RISKY, "push/merge/... from a script"),
]
SETTINGS_FILE = re.compile(r"(^|/)\.agent/settings(\.local)?\.json$")
SETTINGS_CMD = re.compile(r"disableAllHooks|--settings\b|allowManagedHooksOnly")
HOW = (" Don't work around it: tell the user what you were doing, and if you think the rule is wrong, propose the"
       " exact hook change with its pros, cons and how often it would help - the user decides.")


# ---------- zones ----------
def load_zones():
    z = json.load(open(ZONES, encoding="utf-8"))
    unlocked = set()
    if os.path.exists(UNLOCK):
        unlocked = set(json.load(open(UNLOCK, encoding="utf-8")).keys())
    locked = {n: v["paths"] for n, v in z["locked"].items() if n not in unlocked}
    readonly = {n: v["paths"] for n, v in z["readonly"].items() if n not in unlocked}
    return locked, readonly, z.get("search_roots", []), [re.compile(p) for p in z.get("bash_allow", [])]


def to_abs(path, cwd):
    p = path.strip().strip("'\"").replace("\\", "/")
    if re.match(r"^[A-Za-z]:/", p):
        p = "/mnt/" + p[0].lower() + p[2:]
    p = os.path.expanduser(p)
    if not p.startswith("/"):
        p = os.path.join(cwd, p)
    return os.path.normpath(p)


def to_rel(path, cwd):
    """Project-relative path ('.' = root), or None if outside the project."""
    p = to_abs(path, cwd)
    if p == ROOT:
        return "."
    if p.startswith(ROOT + "/"):
        return p[len(ROOT) + 1:]
    return None


def zone_of(rel, zones):
    if rel is None or rel == ".":
        return None
    for name, paths in zones.items():
        for z in paths:
            if z.endswith("/") and (rel + "/").startswith(z):
                return name
            if not z.endswith("/") and rel == z:
                return name
    return None


def zone_inside(rel, zones):
    """A zone that lies inside folder rel (a recursive walk of rel would enter it), or None."""
    if rel is None:
        return None
    base = "" if rel == "." else rel.rstrip("/") + "/"
    for name, paths in zones.items():
        for z in paths:
            if z.startswith(base) and z != base:
                return name
    return None


def expand(arg, cwd):
    """A path argument with wildcards -> the paths it matches on disk now (deterministic for a given disk)."""
    if not GLOB_CH.search(arg):
        return [arg]
    hits = glob.glob(to_abs(arg, cwd), recursive=True)
    return hits or [arg.split("*")[0].split("?")[0].split("[")[0] or "."]


# ---------- checks ----------
def check_settings(abs_path):
    if SETTINGS_FILE.search(abs_path):
        return "HARD GATE: '%s' is a Agent settings file (could turn the hooks off) - only the user changes it" % abs_path
    return None


def check_path(rel, mode, locked, readonly, abs_path=""):
    if mode == "edit" and abs_path:
        hit = check_settings(abs_path)
        if hit:
            return hit
    if rel is None:
        return None
    n = zone_of(rel, locked)
    if n:
        return "'%s' is in the locked zone '%s'" % (rel, n)
    if mode == "edit":
        if rel == ".agent/zones.unlock":
            return "only the user can unlock zones (! .agent/zone.sh unlock <zone>)"
        n = zone_of(rel, readonly)
        if n:
            return "'%s' is in the read-only zone '%s'" % (rel, n)
    return None


def check_walk(rel, locked, roots):
    hit = check_path(rel, "read", locked, {})
    if hit:
        return hit
    n = zone_inside(rel, locked)
    if n:
        return ("a recursive search of '%s' would walk into the locked zone '%s' - point it at one of: %s"
                % (rel, n, ", ".join(roots)))
    return None


def segments(cmd):
    """Split a shell command at && || ; | and new lines - but only OUTSIDE quotes (sed 's|a|b|' stays one piece)."""
    out, cur, q, i = [], [], None, 0
    while i < len(cmd):
        ch = cmd[i]
        if q:
            cur.append(ch)
            if ch == q:
                q = None
            elif ch == "\\" and q == '"' and i + 1 < len(cmd):
                cur.append(cmd[i + 1]); i += 1
        elif ch in "'\"":
            q = ch; cur.append(ch)
        elif cmd.startswith(("&&", "||"), i):
            out.append("".join(cur)); cur = []; i += 1
        elif ch in ";|\n":
            out.append("".join(cur)); cur = []
        else:
            cur.append(ch)
        i += 1
    out.append("".join(cur))
    return out


def tokens(seg):
    try:
        return shlex.split(seg)
    except ValueError:
        return seg.split()


def search_targets(tok):
    """Recursive-search command -> its target paths ([] if it isn't one)."""
    while tok and re.match(r"^\w+=", tok[0]):
        tok = tok[1:]
    if not tok:
        return []
    prog, args = os.path.basename(tok[0]), tok[1:]
    if prog == "git" and args[:1] == ["grep"]:
        rest = args[1:]
        return rest[rest.index("--") + 1:] if "--" in rest else ["."]
    opts = [a for a in args if a.startswith("-")]
    pos = [a for a in args if not a.startswith("-")]
    if prog in ("grep", "egrep", "fgrep"):
        if not any(re.match(r"^-[a-zA-Z]*[rR]", o) or o in ("--recursive", "--dereference-recursive") for o in opts):
            return []
        if not any(o == "-e" or o.startswith("--regexp") or o == "-f" for o in opts):
            pos = pos[1:]
        return pos or ["."]
    if prog == "rg":
        if "--files" not in opts and "-e" not in opts:
            pos = pos[1:]
        return pos or ["."]
    if prog == "find":
        paths = []
        for a in args:
            if a.startswith("-") or a in ("(", "!", "\\("):
                break
            paths.append(a)
        return paths or ["."]
    if prog == "tree" or (prog == "ls" and any(re.match(r"^-[a-zA-Z]*R", o) or o == "--recursive" for o in opts)):
        return pos or ["."]
    return []


def write_targets(seg, tok):
    """Paths a command segment would write/delete (redirects, cp/mv dest, rm/touch/…, sed -i, tee, open(...,'w'))."""
    out = re.findall(r">>?\s*([^\s;&|<>]+)", seg)
    out += re.findall(r"open\(\s*['\"]([^'\"]+)['\"]\s*,\s*['\"][wax]", seg)
    if tok:
        prog = os.path.basename(tok[0])
        args = [a for a in tok[1:] if not a.startswith("-")]
        if prog in ("cp", "mv", "install", "ln", "rsync") and args:
            out.append(args[-1])
        elif prog in ("rm", "rmdir", "touch", "truncate", "chmod", "chown", "tee", "unlink", "shred"):
            out += args
        elif prog == "sed" and any(a.startswith("-i") or a == "--in-place" for a in tok[1:]):
            out += args[1:]
    return out


def mention_patterns(zones, cwd):
    out = []
    crel = to_rel(cwd, cwd)
    for name, paths in zones.items():
        for z in paths:
            zp = z.rstrip("/")
            forms = {ROOT + "/" + zp, zp}
            if crel not in (None, ".") and (zp + "/").startswith(crel + "/"):
                forms.add(zp[len(crel) + 1:])
            end = r"(?=/|$|[\s'\"`;&|)<>])" if z.endswith("/") else r"(?=$|[\s'\"`;&|)<>])"
            for f in forms:
                if f:   # a relative form must START a path (not sit inside another one, e.g. .scratch/x/.agent/hooks)
                    start = r"(?<![\w.-])" if f.startswith("/") else r"(?<![\w./-])"
                    out.append((name, z, re.compile(start + re.escape(f) + end)))
    return out


HEREDOC = re.compile(r"<<(-?)\s*(['\"])(\w+)\2")


def strip_data_heredocs(c):
    """Blank the bodies of quoted heredocs that only SAVE text (`cat > f <<'X'`, `cat >> f`, `tee f`): a note or doc
    that names a locked folder isn't a path the command uses (H3, 2026-10-27). Kept (still checked): heredocs fed to
    anything else (python, bash, ...), unquoted ones ($(...) runs inside them), piped ones, and a saved file that the
    same command runs afterwards. The target file name stays in the line, so it's still checked as a write."""
    lines = c.split("\n")
    out, i = [], 0
    while i < len(lines):
        line = lines[i]
        out.append(line)
        m = HEREDOC.search(line)
        i += 1
        if not m:
            continue
        head, tail, delim, dash = line[:m.start()], line[m.end():], m.group(3), m.group(1)
        end = i
        while end < len(lines) and (lines[end].lstrip("\t") if dash else lines[end]) != delim:
            end += 1
        tok = tokens(segments(head)[-1].strip())
        prog = os.path.basename(tok[0]) if tok else ""
        target = None
        if "|" not in tail:
            if prog == "cat":
                t = re.findall(r">>?\s*([^\s;&|<>]+)", head + tail)
                target = t[0] if t else None
            elif prog == "tee":
                t = [a for a in tok[1:] if not a.startswith("-")]
                target = t[0] if t else None
        rest = "\n".join(lines[end + 1:])
        if target and target.strip("'\"") not in rest:
            out += [""] * (end - i)   # body blanked, line count kept
            i = end
    return "\n".join(out)


INTERP = re.compile(r"^(python[\d.]*|python\.exe|bash|sh|zsh|node|perl|ruby)$")


def agent_script(tok, cwd):
    """The script file a command segment runs, if the agent wrote it (in .scratch/ or a /tmp scratchpad), else None.
    Committed tools aren't scanned: they read locked zones on purpose and only change through a reviewed step."""
    while tok and re.match(r"^\w+=", tok[0]):
        tok = tok[1:]
    if not tok:
        return None
    prog, cand = os.path.basename(tok[0]), None
    if INTERP.match(prog):
        for a in tok[1:]:
            if a in ("-c", "-m", "-e"):
                return None   # inline code is part of the command line, which is checked already
            if not a.startswith("-"):
                cand = a
                break
    elif "/" in tok[0]:
        cand = tok[0]
    if not cand:
        return None
    p = to_abs(cand, cwd)
    rel = to_rel(p, cwd)
    return p if (rel and rel.startswith(".scratch/")) or p.startswith("/tmp/") else None


def check_script(path, locked, cwd, allow=()):
    """A script can write where this guard can't look (step 2026-10-27-10: a render script saved images into a locked
    folder). So before running a script the agent wrote, its text gets the same locked-path check as a command."""
    try:
        if os.path.getsize(path) > 1024 * 1024:
            return None
        text = open(path, encoding="utf-8", errors="replace").read().replace("\\", "/")
    except OSError:
        return None   # not written yet: the command that writes it was checked instead
    text = re.sub(re.escape(WIN_ROOT), ROOT, text, flags=re.I)
    for a in allow:   # the same exceptions as a command line (e.g. "run with .scratch/venv/bin/python" in a docstring)
        text = a.sub(" ", text)
    for name, z, rx in mention_patterns(locked, ROOT):
        if rx.search(text):
            return ("the script '%s' names '%s' in the locked zone '%s' - a script can write where the guard can't"
                    " see, so keep its paths out of locked zones (or ask the user to unlock)" % (to_rel(path, cwd) or path, z, name))
    return None


def check_bash(cmd, cwd, locked, readonly, roots, allow):
    c = cmd.replace("\\", "/")
    c = re.sub(re.escape(WIN_ROOT), ROOT, c, flags=re.I)
    for rx, what in GIT_GATES:
        if re.search(rx, c):
            return "HARD GATE: git/GitHub %s - the agent prepares it, the user runs it with '!'" % what
    if SETTINGS_CMD.search(c):
        return "HARD GATE: the command touches hook/settings switches - only the user changes those"
    if re.search(r"(^|[;&|\n(`]|\$\()\s*(bash\s+|sh\s+)?\S*zone\.sh\s+(unlock|lock)\b", c):   # run, not just mentioned
        return "only the user can lock/unlock zones (! .agent/zone.sh unlock <zone>)"
    c = strip_data_heredocs(c)
    if re.search(r"\bstep\.py[\"']?\s+check\s+--zones-ok\b", c):   # the run itself, not a mention (commit messages)
        return "only the user confirms changes inside locked zones (! python3 kalos-dex/tools/step.py check --zones-ok <zone>)"
    for a in allow:
        c = a.sub(" ", c)
    for name, z, rx in mention_patterns(locked, cwd):
        if rx.search(c):
            return "the command touches '%s' in the locked zone '%s'" % (z, name)
    if re.search(r"\bgit\b[^;&|\n]*\s(show|log\b[^;&|\n]*\s(-p|--patch|-u))", c) and not re.search(r"\s--\s|--stat|--name-only|--oneline\s*$", c):
        return "git log -p / git show without a path could print locked files - add '-- <path>' or --stat"
    run_dir = cwd
    for seg in segments(c):
        s = seg.strip()
        m = re.match(r"^cd\s+(.+)$", s)
        if m:
            t = tokens(m.group(1))
            if t:
                run_dir = to_abs(t[0], run_dir)
            continue
        tok = tokens(s)
        script = agent_script(list(tok), run_dir)
        if script:
            hit = check_script(script, locked, run_dir, allow)
            if hit:
                return hit
        for t in search_targets(list(tok)):
            for p in expand(t, run_dir):
                hit = check_walk(to_rel(p, run_dir), locked, roots)
                if hit:
                    return hit
        # wildcards that would expand into a locked zone - only UNQUOTED words, because only those does the shell
        # expand (a quoted regex like 's/.*/x/' is text for sed/grep/python, not a file pattern)
        unquoted = [w for w in re.findall(r"(?:[^\s'\"]|\\.)+|'[^']*'|\"[^\"]*\"", s)[1:] if not w.startswith(("'", '"'))]
        for t in unquoted:
            if GLOB_CH.search(t) and not t.startswith("-"):
                for p in expand(t, run_dir):
                    hit = check_path(to_rel(p, run_dir), "read", locked, {})
                    if hit:
                        return hit + " (via the wildcard '%s')" % t
        for t in write_targets(s, tok):
            for p in expand(t, run_dir):
                hit = check_settings(to_abs(p, run_dir)) or check_path(to_rel(p, run_dir), "edit", {}, readonly)
                if hit:
                    return hit
    return None


def decide(data):
    tool = data.get("tool_name", "")
    if tool not in GUARDED:
        return None, ""
    ti = data.get("tool_input") or {}
    cwd = data.get("cwd") or ROOT
    locked, readonly, roots, allow = load_zones()
    if tool in FILE_TOOLS:
        p = ti.get("file_path") or ti.get("notebook_path") or ""
        return check_path(to_rel(p, cwd), FILE_TOOLS[tool], locked, readonly, to_abs(p, cwd) if p else ""), p
    if tool in SEARCH_TOOLS:
        p = ti.get("path") or cwd
        pat = ti.get("pattern", "")
        if tool == "Glob":
            for hit_path in glob.glob(os.path.join(to_abs(p, cwd), pat), recursive=True):
                hit = check_path(to_rel(hit_path, cwd), "read", locked, {})
                if hit:
                    return hit + " (via the pattern '%s')" % pat, p
            return check_path(to_rel(p, cwd), "read", locked, {}), p
        return check_walk(to_rel(p, cwd), locked, roots), p
    return check_bash(ti.get("command", ""), cwd, locked, readonly, roots, allow), ti.get("command", "")


def target_of(data):
    ti = data.get("tool_input") or {}
    for k in ("file_path", "notebook_path", "path", "command", "pattern", "url", "description", "skill"):
        if ti.get(k):
            return str(ti[k])
    return ""


def trace(data, decision, why):
    try:
        os.makedirs(os.path.dirname(TRACE), exist_ok=True)
        if os.path.exists(TRACE) and os.path.getsize(TRACE) > 5 * 1024 * 1024:
            os.replace(TRACE, TRACE + ".1")
        with open(TRACE, "a", encoding="utf-8") as f:
            f.write(json.dumps({
                "t": time.strftime("%Y-%m-%dT%H:%M:%S"),
                "session": (data.get("session_id") or "")[:8],
                "agent": data.get("agent_type") or "",
                "agent_id": (data.get("agent_id") or "")[:8],        # tells parallel subagents apart
                "tool": data.get("tool_name", ""),
                "target": target_of(data)[:300],
                "decision": decision,
                "why": why or "",
            }, ensure_ascii=False) + "\n")
    except OSError:
        pass


def main():
    raw = sys.stdin.read()
    try:
        data = json.loads(raw)
    except ValueError:
        data = {}
    try:
        why, _ = decide(data)
    except Exception as e:                                   # fail closed for guarded tools
        why = ("zone guard error (%s: %s) - ask the user to check .agent/ (! git checkout .agent/)"
               % (type(e).__name__, e)) if data.get("tool_name") in GUARDED else None
    trace(data, "deny" if why else "allow", why)
    if why:
        print(json.dumps({"hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "deny",
            "permissionDecisionReason": "ZONE GUARD: " + why + "." + HOW,
        }}))
    sys.exit(0)


if __name__ == "__main__":
    main()

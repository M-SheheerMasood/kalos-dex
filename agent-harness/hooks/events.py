#!/usr/bin/env python3
"""Session hooks for Kalos Dex: state on disk, context nudges, journal, size check, re-lock.

usage (from .agent/settings.json):  python3 events.py <event>      hook JSON on stdin
  session_start  SessionStart   feed STATE.md + unlocked zones + harness version to the agent (startup/resume/compact)
  prompt         UserPromptSubmit  one-line nudge when context is >= 60 % (once per 10 % band)
  stop           Stop           journal line every turn; ask once for a STATE.md update when work changed after it
  pre_compact    PreCompact     copy STATE.md to state/history/ before compaction
  session_end    SessionEnd     re-lock every unlocked zone, journal line
  post_size      PostToolUse    warn when a written file passes 1000 lines (chunk it)
Everything is written under .agent/state/ (STATE.md + steps/ by the agent; the rest only by these hooks).
Test: python3 .agent/hooks/test_events.py
"""
import hashlib, json, os, shutil, subprocess, sys, time

HOOKS = os.path.dirname(os.path.abspath(__file__))
AGENT_DIR = os.path.dirname(HOOKS)
ROOT = os.path.dirname(AGENT_DIR)
STATE_DIR = os.path.join(AGENT_DIR, "state")
STATE = os.path.join(STATE_DIR, "STATE.md")
JOURNAL = os.path.join(STATE_DIR, "journal.jsonl")
CTX = os.path.join(STATE_DIR, "ctx.json")              # written by statusline.py
NUDGE = os.path.join(STATE_DIR, "nudge.json")
HASH = os.path.join(STATE_DIR, "harness.hash")         # harness version the running session loaded
UNLOCK = os.path.join(AGENT_DIR, "zones.unlock")
STATE_MAX = 60
SIZE_MAX = 1000
HARNESS_FILES = ["settings.json", "zones.json", "statusline.py", "hooks/zone_guard.py", "hooks/events.py"]
IGNORE_CHANGES = (".agent/state/", ".agent/logs/", ".scratch/")
STATE_TEMPLATE = """# STATE – live, keep <= 60 lines (fed to every new / resumed / compacted session)
step: none · phase: idle
goal:
decisions:
open questions:
next action:
"""


def now():
    return time.strftime("%Y-%m-%dT%H:%M:%S")


def emit(event, text):
    print(json.dumps({"hookSpecificOutput": {"hookEventName": event, "additionalContext": text}}))


def journal(entry):
    os.makedirs(STATE_DIR, exist_ok=True)
    if os.path.exists(JOURNAL) and os.path.getsize(JOURNAL) > 5 * 1024 * 1024:
        os.replace(JOURNAL, JOURNAL + ".1")
    with open(JOURNAL, "a", encoding="utf-8") as f:
        f.write(json.dumps(dict({"t": now()}, **entry), ensure_ascii=False) + "\n")


def harness_hash():
    h = hashlib.sha1()
    for f in HARNESS_FILES:
        p = os.path.join(AGENT_DIR, f)
        if os.path.exists(p):
            h.update(open(p, "rb").read())
    return h.hexdigest()[:10]


def unlocked():
    try:
        return sorted(json.load(open(UNLOCK, encoding="utf-8")))
    except (OSError, ValueError):
        return []


def git(*args):
    try:
        return subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True, timeout=8).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def changed_files():
    out = []
    for line in git("status", "--porcelain", "--untracked-files=all").splitlines():
        path = line[3:].split(" -> ")[-1].strip('"')
        if path and not path.startswith(IGNORE_CHANGES):
            out.append(path)
    return out


def ctx_pct():
    try:
        return json.load(open(CTX, encoding="utf-8")).get("pct")
    except (OSError, ValueError):
        return None


# ---------- events ----------
def session_start(data):
    os.makedirs(os.path.join(STATE_DIR, "steps"), exist_ok=True)
    if not os.path.exists(STATE):
        open(STATE, "w", encoding="utf-8").write(STATE_TEMPLATE)
    hh = harness_hash()
    open(HASH, "w").write(hh)
    for p in (NUDGE,):
        if os.path.exists(p):
            os.remove(p)
    lines = open(STATE, encoding="utf-8").read().splitlines()
    text = "\n".join(lines[:STATE_MAX])
    if len(lines) > STATE_MAX:
        text += "\n(STATE.md has %d lines - trim it to %d)" % (len(lines), STATE_MAX)
    extra = ["[kalos harness %s · session %s]" % (hh, data.get("source", "?"))]
    if unlocked():
        extra.append("Unlocked zones (by the user): " + ", ".join(unlocked()))
    journal({"event": "session_start", "session": (data.get("session_id") or "")[:8],
             "source": data.get("source", ""), "harness": hh})
    emit("SessionStart", "\n".join(extra) + "\n" + text)


def prompt(data):
    pct = ctx_pct()
    if pct is None or pct < 60:
        return
    band = int(pct // 10) * 10
    try:
        last = json.load(open(NUDGE, encoding="utf-8"))
    except (OSError, ValueError):
        last = {}
    sid = (data.get("session_id") or "")[:8]
    if last.get("session") == sid and last.get("band", 0) >= band:
        return
    json.dump({"session": sid, "band": band}, open(NUDGE, "w", encoding="utf-8"))
    level = "recommend /compact at the next clean point" if pct < 80 else "compact NOW (after saving STATE.md)"
    emit("UserPromptSubmit", "Context is at %d %% - %s. Tell the user in one line." % (pct, level))


def stop(data):
    files = changed_files()
    last = git("log", "-1", "--format=%h %s").strip()[:80]
    journal({"event": "stop", "session": (data.get("session_id") or "")[:8], "ctx": ctx_pct(),
             "dirty": len(files), "files": files[:20], "last_commit": last})
    if data.get("stop_hook_active") or not files:
        return
    state_m = os.path.getmtime(STATE) if os.path.exists(STATE) else 0
    newest = max((os.path.getmtime(os.path.join(ROOT, f)) for f in files if os.path.exists(os.path.join(ROOT, f))),
                 default=0)
    if newest > state_m:
        print(json.dumps({"decision": "block", "reason":
              "Files changed since .agent/state/STATE.md was last updated (%s). Update STATE.md in a few lines "
              "(step, phase, decisions, next action), then finish." % ", ".join(files[:5])}))


def pre_compact(data):
    hist = os.path.join(STATE_DIR, "history")
    os.makedirs(hist, exist_ok=True)
    if os.path.exists(STATE):
        shutil.copy2(STATE, os.path.join(hist, time.strftime("%Y%m%d-%H%M%S") + "-STATE.md"))
    journal({"event": "pre_compact", "session": (data.get("session_id") or "")[:8],
             "trigger": data.get("trigger", ""), "ctx": ctx_pct()})


def session_end(data):
    was = unlocked()
    if os.path.exists(UNLOCK):
        os.remove(UNLOCK)
    journal({"event": "session_end", "session": (data.get("session_id") or "")[:8],
             "reason": data.get("reason", ""), "relocked": was})


def post_size(data):
    p = (data.get("tool_input") or {}).get("file_path") or ""
    if not p or not os.path.isfile(p):
        return
    try:
        n = sum(1 for _ in open(p, encoding="utf-8", errors="replace"))
    except OSError:
        return
    if n > SIZE_MAX:
        emit("PostToolUse", "%s now has %d lines (> %d). Chunk it: docs/logs -> a folder with index.md "
             "(section -> file -> line range) + chunks; code -> split by responsibility. Ask the user first."
             % (os.path.relpath(p, ROOT), n, SIZE_MAX))


EVENTS = {"session_start": session_start, "prompt": prompt, "stop": stop, "pre_compact": pre_compact,
          "session_end": session_end, "post_size": post_size}


def main():
    try:
        data = json.loads(sys.stdin.read() or "{}")
    except ValueError:
        data = {}
    try:
        EVENTS[sys.argv[1]](data)
    except Exception as e:      # never block the session because of a bookkeeping error; leave a trace
        try:
            journal({"event": "hook_error", "hook": sys.argv[1:2], "error": "%s: %s" % (type(e).__name__, e)})
        except Exception:
            pass
    sys.exit(0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Prove a step is finished, then close it.  (stdlib only, run from WSL)

  step.py check [--zones-ok Z[,Z]]      run every check; on ALL PASS remember a fingerprint of the tree.
                                        First: nothing changed inside a LOCKED zone (a script can write where the
                                        guard can't see). --zones-ok accepts changes in those zones: the user's
                                        flag only (the guard denies it to the agent).
  step.py done <step-id> "what changed" [--add NEW_FILE ...]
                                        refuses unless the tree is exactly what passed `check` and every new
                                        file is named; then: result into the step file, a session-log line
                                        (unless one with the id exists), one commit "<id>: what changed · ..."

New files are never committed silently: `done` lists them and stops - ask the user, then pass --add.
This never pushes (pushes are the user's, behind the hard gate).
"""
import hashlib, json, os, subprocess, sys, time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FW = ROOT / "kalos-dex/firmware/kalos-dex"
PIO = "pio"   # PlatformIO CLI (the private copy has the full path to it here)
LAST = ROOT / ".agent/state/last_check.json"
STEPS = ROOT / ".agent/state/steps"
LOG = ROOT / "kalos-dex/docs/session-log.md"
TRAILER = "Co-Authored-By: <the agent> <noreply@example.com>"

CHECKS = [  # name, command, folder
    ("guard tests",   ["python3", ".agent/hooks/test_zone_guard.py"], ROOT),
    ("events tests",  ["python3", ".agent/hooks/test_events.py"], ROOT),
    ("codemap fresh", ["python3", "kalos-dex/tools/codemap.py", "--check"], ROOT),
    ("build esp32dev", [PIO, "run", "-e", "esp32dev"], FW),
    ("build hwtest",  [PIO, "run", "-e", "hwtest"], FW),
]


def git(*args, binary=False):
    r = subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True)
    if r.returncode:
        sys.exit(f"git {' '.join(args)} failed: {r.stderr.decode(errors='replace').strip()}")
    return r.stdout if binary else r.stdout.decode(errors="replace")


def new_files():
    return [f for f in git("ls-files", "--others", "--exclude-standard").splitlines() if f]


def fingerprint():   # tracked changes vs HEAD + every new (not ignored) file's name and content
    h = hashlib.sha1(git("rev-parse", "HEAD", binary=True) + git("diff", "HEAD", "--binary", binary=True))
    for f in sorted(new_files()):
        h.update(f.encode() + b"\0" + (ROOT / f).read_bytes())
    return h.hexdigest()[:12]


def zone_changes():   # changed / new / deleted files inside zones that are locked right now
    zones = json.loads((ROOT / ".agent/zones.json").read_text())
    unlock = ROOT / ".agent/zones.unlock"
    unlocked = set(json.loads(unlock.read_text())) if unlock.exists() else set()
    locked = {n: v["paths"] for n, v in zones["locked"].items() if n not in unlocked}
    hits = []
    for line in git("status", "--porcelain", "--untracked-files=all").splitlines():
        f = line[3:].split(" -> ")[-1].strip('"')
        for n, paths in locked.items():
            if any((f + "/").startswith(z) if z.endswith("/") else f == z for z in paths):
                hits.append((n, f))
    pub = ROOT / "public-repo"   # ignored here, its own git: check it the same way while 'sealed' is locked
    if "sealed" in locked and (pub / ".git").exists():
        r = subprocess.run(["git", "-C", str(pub), "status", "--porcelain"], capture_output=True, text=True)
        hits += [("sealed", "public-repo/" + l[3:]) for l in r.stdout.splitlines() if l.strip()]
    return hits   # not covered: git-ignored locked folders (.pio, _archive) - build output changes there all the time


def summary(out):   # the one line worth reading from a check's output
    lines = [l.strip() for l in out.splitlines() if l.strip()]
    for l in reversed(lines):
        if "ALL PASS" in l or "FAIL" in l or "codemap" in l or "SUCCESS" in l or "ERROR" in l or "error:" in l:
            return l.strip("= ")[:90]
    return lines[-1][:90] if lines else "(no output)"


def check(zones_ok=()):
    fp, rows = fingerprint(), []
    hits = [(n, f) for n, f in zone_changes() if n not in zones_ok]
    rows.append({"check": "locked zones", "ok": not hits, "s": 0,
                 "says": ("unchanged" + (" (user OK: %s)" % ",".join(zones_ok) if zones_ok else "")) if not hits
                 else "%d change(s) in locked zones" % len(hits)})
    ok_all = not hits
    print(f"{'PASS' if not hits else 'FAIL'}  {'locked zones':<15} {0:>5}s  {rows[-1]['says']}")
    for n, f in hits[:20]:
        print(f"      {n}: {f}")
    if hits:
        print("      -> tell the user; only they clear it: unlock that zone, or run it themselves with "
              "--zones-ok " + ",".join(sorted({n for n, _ in hits})))
    for name, cmd, cwd in CHECKS:
        t = time.time()
        r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, errors="replace")
        ok = r.returncode == 0
        ok_all &= ok
        rows.append({"check": name, "ok": ok, "s": round(time.time() - t, 1), "says": summary(r.stdout + r.stderr)})
        print(f"{'PASS' if ok else 'FAIL'}  {name:<15} {rows[-1]['s']:>5}s  {rows[-1]['says']}")
        if not ok and "build" in name:   # a failed build: show the compiler errors, not just the last line
            print("\n".join(l for l in (r.stdout + r.stderr).splitlines() if "error" in l.lower())[:2000])
    if fingerprint() != fp:
        sys.exit("FAIL  the tree changed while the checks ran (generated files?) - nothing remembered")
    if ok_all:
        LAST.write_text(json.dumps({"fingerprint": fp, "time": datetime.now().isoformat(timespec="seconds"),
                                    "rows": rows}, indent=1))
        print(f"ALL PASS - remembered tree {fp}; close the step with: step.py done <id> \"what changed\"")
    else:
        LAST.unlink(missing_ok=True)
        sys.exit("NOT DONE - fix the FAIL lines and run check again")


def done(step_id, what, add):
    try:
        last = json.loads(LAST.read_text())
    except (OSError, ValueError):
        sys.exit("refused: no passing `check` on record - run step.py check first")
    own_step = f"{STEPS.relative_to(ROOT)}/{step_id}"   # this step's own new step file needs no --add
    unnamed = [f for f in new_files() if f not in add and not f.startswith(own_step)]
    if unnamed:
        sys.exit("refused: new files not named - ask the user, then add them with --add:\n  " + "\n  ".join(unnamed))
    missing = [f for f in add if not (ROOT / f).exists()]
    if missing:
        sys.exit("refused: --add names files that don't exist: " + ", ".join(missing))
    if fingerprint() != last["fingerprint"]:
        sys.exit(f"refused: the tree changed since the check passed at {last['time']} - run step.py check again")

    tested = ", ".join(r["check"] for r in last["rows"])
    step_files = sorted(STEPS.glob(step_id + "*.md"))
    if step_files:
        with open(step_files[0], "a") as f:
            f.write(f"\n**Result ({last['time']}):** {what}. step.py check ALL PASS:\n"
                    + "".join(f"- {r['check']}: {r['says']} ({r['s']} s)\n" for r in last["rows"]))
    log = LOG.read_text()
    if step_id not in log:
        where = f" · `{step_files[0].relative_to(ROOT)}`" if step_files else ""
        LOG.write_text(log.rstrip("\n") + f"\n- **{step_id} ({last['time'][:10]}):** {what} · step.py check ALL PASS{where}\n")

    git("add", "-u")
    if add:
        git("add", "--", *add)
    for f in step_files:   # a new step file is part of the step (written by the plan, named by the id)
        git("add", "--", str(f.relative_to(ROOT)))
    git("commit", "-q", "-m", f"{step_id}: {what} · tested: step.py check ({tested})\n\n{TRAILER}")
    LAST.unlink(missing_ok=True)
    print(git("log", "--oneline", "-1").strip())
    print("git status:", git("status", "--short").strip() or "clean")


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[:1] == ["check"] and len(a) == 1:
        check()
    elif a[:1] == ["check"] and len(a) == 3 and a[1] == "--zones-ok":
        check(tuple(a[2].split(",")))
    elif a[:1] == ["done"] and len(a) >= 3 and (len(a) == 3 or a[3] == "--add"):
        done(a[1], a[2], a[4:])
    else:
        sys.exit(__doc__)

#!/usr/bin/env python3
"""Fixed cases for events.py + statusline.py, run in a temporary state folder (never the real one).
Run: python3 .agent/hooks/test_events.py   -> PASS/FAIL per case, exit 1 on any failure."""
import contextlib, io, json, os, subprocess, sys, tempfile, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import events as ev

TMP = tempfile.mkdtemp()
ev.STATE_DIR = os.path.join(TMP, "state")
ev.STATE = os.path.join(ev.STATE_DIR, "STATE.md")
ev.JOURNAL = os.path.join(ev.STATE_DIR, "journal.jsonl")
ev.CTX = os.path.join(ev.STATE_DIR, "ctx.json")
ev.NUDGE = os.path.join(ev.STATE_DIR, "nudge.json")
ev.HASH = os.path.join(ev.STATE_DIR, "harness.hash")
ev.UNLOCK = os.path.join(TMP, "zones.unlock")
ev.ROOT = TMP
fails, n = 0, 0


def run(fn, data):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        fn(data)
    out = buf.getvalue().strip()
    return json.loads(out) if out else None


def check(name, ok, info=""):
    global fails, n
    n += 1
    fails += not ok
    print("%s %2d %s%s" % ("PASS" if ok else "FAIL", n, name, "" if ok else "   <- " + str(info)[:200]))


def ctx(pct):
    os.makedirs(ev.STATE_DIR, exist_ok=True)
    json.dump({"pct": pct}, open(ev.CTX, "w"))


def journal_last():
    return json.loads(open(ev.JOURNAL).read().splitlines()[-1])


# session_start
o = run(ev.session_start, {"session_id": "aaaa1111", "source": "startup"})
check("session_start creates STATE.md from the template", os.path.exists(ev.STATE))
check("session_start feeds STATE to the agent", o and "STATE" in o["hookSpecificOutput"]["additionalContext"], o)
check("session_start records the harness version", open(ev.HASH).read() == ev.harness_hash())
open(ev.STATE, "w").write("\n".join("line %d" % i for i in range(70)))
o = run(ev.session_start, {"session_id": "aaaa1111", "source": "compact"})
txt = o["hookSpecificOutput"]["additionalContext"]
check("long STATE.md is cut to 60 lines + trim note", "line 59" in txt and "line 60" not in txt and "trim it" in txt, txt[-80:])
json.dump({"hardware": "t"}, open(ev.UNLOCK, "w"))
o = run(ev.session_start, {"session_id": "aaaa1111", "source": "resume"})
check("session_start lists unlocked zones", "Unlocked zones" in o["hookSpecificOutput"]["additionalContext"])

# prompt nudges
S = {"session_id": "bbbb2222"}
ctx(59); check("59 %: no nudge", run(ev.prompt, S) is None)
ctx(61); o = run(ev.prompt, S); check("61 %: nudge", o and "61 %" in o["hookSpecificOutput"]["additionalContext"], o)
ctx(65); check("65 %: same band, no repeat", run(ev.prompt, S) is None)
ctx(72); check("72 %: next band, nudge", run(ev.prompt, S) is not None)
ctx(85); o = run(ev.prompt, S); check("85 %: 'compact NOW'", o and "NOW" in o["hookSpecificOutput"]["additionalContext"], o)
check("85 % again: no repeat", run(ev.prompt, S) is None)
check("new session at 85 %: nudge again", run(ev.prompt, {"session_id": "cccc3333"}) is not None)

# stop
work = os.path.join(TMP, "a.txt")
ev.changed_files = lambda: ["a.txt"]
open(ev.STATE, "w").write(ev.STATE_TEMPLATE)
time.sleep(0.05); open(work, "w").write("x")
o = run(ev.stop, {"session_id": "dddd"})
check("stop: work newer than STATE.md -> block once", o and o.get("decision") == "block", o)
check("stop: journal line written", journal_last()["event"] == "stop" and journal_last()["files"] == ["a.txt"])
check("stop: second try (stop_hook_active) never blocks", run(ev.stop, {"session_id": "dddd", "stop_hook_active": True}) is None)
time.sleep(0.05); open(ev.STATE, "a").write("next action: test\n")
check("stop: STATE.md updated -> no block", run(ev.stop, {"session_id": "dddd"}) is None)
ev.changed_files = lambda: []
check("stop: clean tree -> no block", run(ev.stop, {"session_id": "dddd"}) is None)

# pre_compact, session_end, post_size
run(ev.pre_compact, {"session_id": "eeee", "trigger": "auto"})
check("pre_compact copies STATE.md to history/", len(os.listdir(os.path.join(ev.STATE_DIR, "history"))) == 1)
run(ev.session_end, {"session_id": "eeee", "reason": "exit"})
check("session_end re-locks", not os.path.exists(ev.UNLOCK) and journal_last()["relocked"] == ["hardware"], journal_last())
big = os.path.join(TMP, "big.md"); open(big, "w").write("x\n" * 1001)
small = os.path.join(TMP, "small.md"); open(small, "w").write("x\n" * 10)
o = run(ev.post_size, {"tool_input": {"file_path": big}})
check("post_size: 1001 lines -> chunk warning", o and "1001 lines" in o["hookSpecificOutput"]["additionalContext"], o)
check("post_size: 10 lines -> quiet", run(ev.post_size, {"tool_input": {"file_path": small}}) is None)

# copies of events.py + statusline.py in TMP, so subprocess runs write their state/journal there, never the real one
sl_dir = os.path.join(TMP, "sl"); os.makedirs(os.path.join(sl_dir, "hooks"))
for f in ("statusline.py", "hooks/events.py"):
    src = os.path.join(os.path.dirname(HERE), f)
    open(os.path.join(sl_dir, f), "w").write(open(src).read())

# a broken event never raises out of main()
real_journal = os.path.join(os.path.dirname(HERE), "state", "journal.jsonl")
before = os.path.getsize(real_journal) if os.path.exists(real_journal) else 0
r = subprocess.run([sys.executable, os.path.join(sl_dir, "hooks", "events.py"), "nonsense"], input="{}",
                   capture_output=True, text=True)
check("unknown event: exit 0, no output", r.returncode == 0 and r.stdout == "", r)
check("unknown event: logged in the TEMP journal", "nonsense" in open(os.path.join(sl_dir, "state", "journal.jsonl")).read())
check("the real journal is untouched by the tests",
      (os.path.getsize(real_journal) if os.path.exists(real_journal) else 0) == before)

# statusline
def sl(pct):
    d = {"session_id": "ffff", "context_window": {"used_percentage": pct, "context_window_size": 200000}}
    return subprocess.run([sys.executable, os.path.join(sl_dir, "statusline.py")], input=json.dumps(d),
                          capture_output=True, text=True).stdout
o = sl(42); check("statusline 42 %: plain", "ctx 42%" in o and "\033[3" not in o, repr(o))
o = sl(63); check("statusline 63 %: yellow + 'compact recommended'", "\033[33mctx 63% · compact recommended" in o, repr(o))
o = sl(91); check("statusline 91 %: red + 'compact now'", "\033[31mctx 91% · compact now" in o, repr(o))
check("statusline stores ctx.json", json.load(open(os.path.join(sl_dir, "state", "ctx.json")))["pct"] == 91)
open(os.path.join(sl_dir, "state", "harness.hash"), "w").write("old")
check("statusline flags a harness change", "harness changed" in sl(10), sl(10))
o = subprocess.run([sys.executable, os.path.join(sl_dir, "statusline.py")], input="{}", capture_output=True, text=True).stdout
check("statusline before first reply: 'ctx –'", "ctx –" in o, repr(o))

print("\n%s: %d cases, %d failed" % ("ALL PASS" if not fails else "FAILED", n, fails))
sys.exit(1 if fails else 0)

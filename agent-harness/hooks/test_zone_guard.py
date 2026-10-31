#!/usr/bin/env python3
"""Fixed cases for zone_guard.py: each tool call must get the expected answer (allow / deny).
Run: python3 .agent/hooks/test_zone_guard.py   -> PASS/FAIL per case, exit 1 on any failure.
Uses a temporary unlock file, never the real one, and doesn't write to the real trace log.
(KALOS_TEST_ROOT=<repo> lets a draft copy outside .agent/ be tested against the real repo.)"""
import json, os, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import zone_guard as g
if os.environ.get("KALOS_TEST_ROOT"):
    g.ROOT = os.environ["KALOS_TEST_ROOT"]
    g.ZONES = os.path.join(os.path.dirname(HERE), "zones.json")

R = g.ROOT
FW = R + "/kalos-dex/firmware/kalos-dex"
HOME = os.path.expanduser("~")
A, D = "allow", "deny"

def bash(cmd, cwd=R): return {"tool_name": "Bash", "tool_input": {"command": cmd}, "cwd": cwd}
def tool(name, cwd=R, **ti): return {"tool_name": name, "tool_input": ti, "cwd": cwd}

CASES = [
    # --- file tools ---
    (A, tool("Read", file_path=FW + "/src/pet.cpp")),
    (A, tool("Read", file_path="kalos-dex/CONTEXT.md")),
    (D, tool("Read", file_path=R + "/kalos-dex/scad/kalos_case.scad")),
    (D, tool("Read", file_path=R + "/kalos-dex/docs/log/2026-10-13_to_10-24_build-and-firmware-0.6.md")),
    (D, tool("Read", file_path=R + "/kalos-dex/docs/hardware/wiring-plan.md")),
    (D, tool("Read", file_path=FW + "/.pio/build/esp32dev/firmware.map")),
    (D, tool("Read", file_path="C:\\path\\to\\project\\kalos-dex\\_archive\\README.md")),
    (D, tool("Read", file_path=R + "/.scratch/venv/pyvenv.cfg")),
    (A, tool("Read", file_path=R + "/.scratch/tmp/notes.txt")),
    (A, tool("Read", file_path=R + "/kalos-dex/docs/pinout.md")),
    (D, tool("Edit", file_path=R + "/kalos-dex/docs/pinout.md")),
    (A, tool("Edit", file_path=FW + "/src/pet.cpp")),
    (D, tool("Write", file_path=R + "/.agent/zones.json")),
    (D, tool("Write", file_path=R + "/.agent/zones.unlock")),
    (D, tool("Edit", file_path=R + "/.agent/hooks/zone_guard.py")),
    (D, tool("Edit", file_path=R + "/.agent/hooks/events.py")),
    (D, tool("Edit", file_path=R + "/.agent/statusline.py")),
    (D, tool("Edit", file_path=R + "/.agent/logs/trace.jsonl")),
    (D, tool("Write", file_path=R + "/.agent/state/journal.jsonl")),
    (D, tool("Write", file_path=R + "/.agent/state/history/x-STATE.md")),
    (A, tool("Write", file_path=R + "/.agent/state/STATE.md")),
    (A, tool("Write", file_path=R + "/.agent/state/steps/2026-10-26-01-x.md")),
    (A, tool("Edit", file_path=R + "/.agent/skills/kalos-step/SKILL.md")),
    (A, tool("Read", file_path=R + "/.agent/hooks/zone_guard.py")),
    (D, tool("Write", file_path=R + "/public-repo/README.md")),
    (A, tool("Read", file_path=HOME + "/.agent/projects/x/memory/MEMORY.md")),       # outside the project
    (A, tool("Read", file_path="../kalos_case.scad", cwd="/tmp")),
    # --- settings files: hard gate, anywhere ---
    (D, tool("Write", file_path=R + "/.agent/settings.local.json")),
    (D, tool("Edit", file_path=HOME + "/.agent/settings.json")),
    (A, tool("Read", file_path=HOME + "/.agent/settings.json")),
    (D, bash("echo '{\"disableAllHooks\": true}' > /tmp/x.json")),
    (D, bash("agent --settings /tmp/x.json")),
    (D, bash("python3 -c \"open('%s/.agent/settings.json','w').write('{}')\"" % HOME)),
    # --- search tools ---
    (A, tool("Grep", path=FW + "/src", pattern="x")),
    (D, tool("Grep", pattern="x")),
    (D, tool("Grep", path=FW, pattern="x")),
    (D, tool("Glob", path=R + "/kalos-dex/scad", pattern="*.scad")),
    (A, tool("Glob", path=R + "/kalos-dex/tools", pattern="*.py")),
    (D, tool("Glob", path=R + "/kalos-dex/tools", pattern="**/*.py")),
    (A, tool("Glob", path=R + "/kalos-dex/docs", pattern="*.md")),
    (D, tool("Glob", pattern="**/*.scad")),
    # --- bash: direct mentions ---
    (D, bash("cat kalos-dex/scad/caps.scad")),
    (D, bash("cat scad/caps.scad", cwd=R + "/kalos-dex")),
    (D, bash("cd kalos-dex/scad && ls")),
    (D, bash("ls _archive", cwd=R + "/kalos-dex")),
    (D, bash("head -50 \"%s/kalos-dex/docs/log/2026-10-13_to_10-24_build-and-firmware-0.6.md\"" % R)),
    (D, bash("python3 -c \"print(open('kalos-dex/docs/hardware/wiring-plan.md').read())\"")),
    (A, bash("cat kalos-dex/docs/pinout.md")),
    (D, bash("sed -i s/a/b/ kalos-dex/docs/pinout.md")),
    (D, bash("echo x > .agent/zones.json")),
    (A, bash("cat .agent/zones.json")),
    (A, bash("python3 .agent/hooks/test_zone_guard.py")),
    (D, bash("rm .agent/zones.unlock")),
    (D, bash(".agent/zone.sh unlock hardware")),
    (A, bash(".agent/zone.sh status")),
    (D, bash("bash .agent/zone.sh lock all")),
    (D, bash("cd .agent && ./zone.sh unlock harness")),
    (A, bash("cat > .scratch/tmp/n.md <<'EOF'\nnext: after `! .agent/zone.sh unlock harness` deploy\nEOF")),
    (A, bash("cat .agent/zones.unlock")),
    (D, bash("echo '{}' > .agent/zones.unlock")),
    # --- bash: wildcards (V3) ---
    (D, bash("cat kalos-dex/sc*/caps.scad")),
    (D, bash("cat kalos-dex/docs/*/wiring-plan.md")),
    (A, bash("grep -n pet kalos-dex/docs/*.md")),
    (A, bash("grep -n pet *.md", cwd=R + "/kalos-dex/docs")),
    (A, bash("grep -n x kalos-dex/tools/*.py")),
    # H1 (2026-10-26): only unquoted words are wildcards
    (A, bash("sed -i 's|^step: .*|step: x|' .agent/state/STATE.md")),
    (A, bash("grep -n 'Btn::.*Play' kalos-dex/firmware/kalos-dex/src/main.cpp")),
    (A, bash("python3 -c \"import re; print(re.sub(r'.*', 'x', 'a'))\"")),
    (D, bash("cat .g*/config")),
    (D, bash("ls kalos-dex/sc?d")),
    # H2 (2026-10-26): the scratch env can be RUN, not read
    (A, bash(".scratch/venv/bin/python -c \"import PIL\"")),
    (A, bash("\"%s/.scratch/venv/bin/python\" kalos-dex/tools/clock_preview.py" % R)),
    (A, bash(".scratch/venv/bin/pip install -q pillow")),
    (D, bash("cat .scratch/venv/pyvenv.cfg")),
    (D, bash("ls .scratch/venv/lib")),
    (D, bash(".scratch/venv/bin/python -c \"print(open('.scratch/venv/pyvenv.cfg').read())\"")),
    # --- bash: read-only zones, write targets only (false positives fixed) ---
    (A, bash("cp .agent/zones.json .scratch/tmp/zones.json")),
    (D, bash("cp .scratch/tmp/zones.json .agent/zones.json")),
    (A, bash("ls .scratch/tmp/harness-draft/.agent/hooks", cwd=R)),
    (A, bash("python3 - <<'EOF'\nopen('.scratch/tmp/harness-draft/.agent/hooks/x.py','w')\nEOF")),
    (D, bash("python3 - <<'EOF'\nopen('.agent/logs/trace.jsonl','w')\nEOF")),
    (D, bash("echo x >> .agent/state/journal.jsonl")),
    (A, bash("echo hi > .scratch/tmp/a.txt")),
    # --- bash: git hard gate ---
    (D, bash("git push")),
    (D, bash("git -C public-repo push && git -C public-repo push --tags")),
    (D, bash("git -C \"public-repo\" push origin main")),
    (D, bash("git merge feature")),
    (D, bash("git pull")),
    (D, bash("git rebase main")),
    (D, bash("git reset --hard HEAD~1")),
    (D, bash("git commit --amend -m x")),
    (D, bash("git branch -D old")),
    (D, bash("git tag -d v0.6.0")),
    (D, bash("gh repo edit --visibility private")),
    (D, bash("gh pr merge 3")),
    (A, bash("git commit -m 'fix the push/merge wording in docs'")),
    # docs may mention it, commands may not (user, 2026-10-26)
    (A, bash("git commit -m 'gate: block git push in shell commands'")),
    (A, bash("cat >> .scratch/tmp/doc.md <<'EOF'\nHARD gate for git push/merge/history; the user runs: git -C public-repo push\nEOF")),
    (A, bash("python3 - <<'EOF'\ntext = 'hand the user the line: git -C public-repo push --tags'\nEOF")),
    (A, bash("echo 'later: git push' > .scratch/tmp/n.txt")),
    (D, bash("cd kalos-dex && git push")),
    (D, bash("echo hi; git -C public-repo push")),
    (D, bash("FOO=1 git push")),
    (D, bash("sudo git push")),
    (D, bash("/usr/bin/git push")),
    (D, bash("x=$(git push)")),
    (D, bash("bash -c 'git push origin main'")),
    (D, bash("eval \"git -C public-repo push\"")),
    (D, bash("bash <<'EOF'\ncd public-repo\ngit push\nEOF")),
    (D, bash("python3 -c \"import subprocess; subprocess.run(['git', 'push'])\"")),
    (D, bash("python3 -c \"import subprocess; subprocess.run(['git', '-C', 'public-repo', 'push'])\"")),
    (D, bash("ls | gh repo delete x")),
    (A, bash("git add -A && git commit -q -m 'Step 3: merge notes · tested'")),
    (A, bash("git -C public-repo status")),
    (A, bash("git -C public-repo tag v0.7.0")),
    (A, bash("git status --short")),
    (A, bash("git log --oneline -5")),
    (A, bash("git diff --stat")),
    (D, bash("git log -p")),
    (D, bash("git show HEAD")),
    (A, bash("git show --stat HEAD")),
    (A, bash("git log -p -- kalos-dex/firmware/kalos-dex/src/pet.cpp")),
    (D, bash("git diff kalos-dex/scad/caps.scad")),
    (A, bash("git add kalos-dex/scad/caps.scad && git commit -m 'x'")),
    (D, bash("cat public-repo/README.md")),
    (A, bash("ls -la .pio/build/esp32dev/firmware.bin", cwd=FW)),
    (D, bash("ls .pio/build/esp32dev/", cwd=FW)),
    # --- bash: recursive searches ---
    (A, bash("grep -rn TOP_INSET src include", cwd=FW)),
    (D, bash("grep -rn pet .", cwd=R + "/kalos-dex/docs")),
    (D, bash("grep -rn TOP_INSET .", cwd=FW)),
    (D, bash("grep -rn foo")),
    (D, bash("rg foo")),
    (A, bash("rg foo src", cwd=FW)),
    (D, bash("find . -name '*.stl'")),
    (A, bash("find src -name '*.cpp'", cwd=FW)),
    (D, bash("cd kalos-dex && grep -rn x tools")),
    (D, bash("git grep foo")),
    (A, bash("git grep foo -- kalos-dex/firmware/kalos-dex/src")),
    (D, bash("ls -R kalos-dex")),
    (A, bash("ls kalos-dex")),
    (A, bash("grep -c x kalos-dex/CONTEXT.md")),
    (A, bash("grep -rn x .agent/skills")),
    # --- tools that aren't guarded ---
    (A, {"tool_name": "Agent", "tool_input": {"prompt": "read kalos-dex/scad"}, "cwd": R}),
]

def main():
    tmp = tempfile.mkdtemp()
    g.UNLOCK = os.path.join(tmp, "zones.unlock")
    fails = 0
    for i, (want, data) in enumerate(CASES, 1):
        why, _ = g.decide(data)
        got = D if why else A
        ok = got == want
        fails += not ok
        ti = data["tool_input"]
        what = data["tool_name"] + " " + (ti.get("command") or ti.get("file_path") or ti.get("path") or "(cwd)")
        what = what.replace(R, "<R>").replace("\n", "⏎")
        print("%s %3d %-5s %s%s" % ("PASS" if ok else "FAIL", i, got, what[:95], "" if ok else "   <- wanted %s (%s)" % (want, why)))
    # scripts the agent wrote are read before they run; heredocs that only save text aren't paths (2026-10-27, step 11)
    sd = R + "/.scratch/tmp/zg-test"
    os.makedirs(sd, exist_ok=True)
    files = {sd + "/bad.py": 'OUT = ROOT / "kalos-dex/_archive/x.png"\n',
             sd + "/good.py": '"""run with .scratch/venv/bin/python"""\nOUT = ROOT / ".scratch/previews/x.png"\n',
             sd + "/bad.sh": "rm -rf kalos-dex/_archive\n",
             tmp + "/bad2.py": "open('kalos-dex/_archive/a', 'w')\n"}
    for p, t in files.items():
        open(p, "w").write(t)
    scripts = [
        (D, bash("python3 .scratch/tmp/zg-test/bad.py")),
        (A, bash(".scratch/venv/bin/python .scratch/tmp/zg-test/good.py")),
        (D, bash("bash .scratch/tmp/zg-test/bad.sh")),
        (D, bash("cd .scratch/tmp/zg-test && python3 -I bad.py")),
        (D, bash("python3 " + tmp + "/bad2.py")),
        (A, bash("python3 .scratch/tmp/zg-test/not-written-yet.py")),
        (A, bash('python3 -c "print(1)"')),
        (A, bash("python3 public-draft/build_public.py --out .scratch/public-preview")),   # committed tool: not scanned
        (A, bash("cat >> notes.md <<'EOF'\nthe old files are in kalos-dex/_archive/\nEOF")),
        (A, bash("tee notes.md <<'EOF'\nsee kalos-dex/_archive/README.md\nEOF")),
        (D, bash("python3 - <<'EOF'\nopen('kalos-dex/_archive/x', 'w')\nEOF")),
        (D, bash("cat <<'EOF' | python3\nprint(open('kalos-dex/_archive/a').read())\nEOF")),
        (D, bash("cat > .scratch/tmp/zg-test/run.py <<'EOF'\nopen('kalos-dex/_archive/x', 'w')\nEOF\npython3 .scratch/tmp/zg-test/run.py")),
        (D, bash("cat > notes.md <<EOF\n$(ls kalos-dex/_archive)\nEOF")),
        (D, bash("cat > kalos-dex/_archive/n.md <<'EOF'\nhi\nEOF")),
        (D, bash("cat >> notes.md <<'EOF'\nok\nEOF\ngit push")),
        (D, bash("python3 kalos-dex/tools/step.py check --zones-ok hardware")),
        (A, bash("python3 kalos-dex/tools/step.py check")),
        # H4 (2026-10-28): a `*` inside a quoted heredoc is text, never a file pattern
        (A, bash("cat > .scratch/tmp/zg-test/m.py <<'EOF'\nw = 3 * 4\nrows = [[1] * w for _ in range(2)]\nEOF")),
        (A, bash("cat > .scratch/tmp/zg-test/m.py <<'EOF'\nx = a * b\nEOF\npython3 .scratch/tmp/zg-test/m.py")),
        (A, bash("python3 - <<'EOF'\nprint(2 * 3)\nEOF")),
        (D, bash("python3 - <<'EOF'\nprint(2 * 3)\nopen('kalos-dex/_archive/x', 'w')\nEOF")),   # the text check stays
        (D, bash("ls *")),                                                              # outside a heredoc: still a wildcard
        (D, bash("cat > n.md <<EOF\nls *\nEOF")),                                         # unquoted heredoc: still checked
        (D, bash("python3 - <<'EOF'\nopen('.agent/hooks/zone_guard.py', 'w')\nEOF")),  # write into read-only: still seen
        # G1 (2026-10-28): a `*` inside $( ) / backticks is a wildcard too
        (D, bash("echo $(ls *)")),
        (D, bash("x=$(ls *)")),
        (D, bash("echo `ls *`")),
        (A, bash("echo $(ls kalos-dex/firmware/kalos-dex/src/*.cpp)")),
        (A, bash('python3 kalos-dex/tools/step.py done 2026-10-27-11 "check fails on locked zones (user-only --zones-ok)"')),
        # 2026-10-29: the public build writes into the sealed clone unless --out (an unknown flag once ran it for real)
        (D, bash("python3 public-draft/build_public.py")),
        (D, bash("python3 public-draft/build_public.py --bogus")),
        (D, bash("python3 public-draft/build_public.py -x --out .scratch/p")),
        (D, bash("cd public-draft && python3 build_public.py 2>&1 | tail -2")),
        (D, bash("timeout 60 python3 -I public-draft/build_public.py")),
        (D, bash("X=1 python3 public-draft/build_public.py > .scratch/log.txt")),
        (D, bash("public-draft/build_public.py")),
        (A, bash("python3 public-draft/build_public.py --out .scratch/public-preview 2>&1 | tail -2")),
        (A, bash("python3 public-draft/build_public.py --help")),
        (A, bash("grep -n argv public-draft/build_public.py")),                  # a mention, not a run
        (A, bash("sed -n 1,20p public-draft/build_public.py")),
        (A, bash('git commit -m "build_public.py: refuse unknown flags"')),
        # H5 (2026-10-29): only real git runs print files; git show <rev>:<file> = one file, judged by its zone
        (A, bash("git show HEAD:.agent/hooks/zone_guard.py > .scratch/tmp/old.py")),
        (A, bash("git show HEAD~2:kalos-dex/firmware/kalos-dex/src/pet.cpp | head")),
        (D, bash("git show HEAD:kalos-dex/_archive/README.md")),
        (D, bash("git -C . show HEAD:kalos-dex/scad/kalos_case.scad")),
        (A, bash("python3 - <<'EOF'\nnote = 'the git show rule blocked a note'\nEOF")),
        (A, bash("grep -n 'git log -p / git show' .agent/hooks/zone_guard.py")),
        (A, bash('git commit -m "guard: git show HEAD:file allowed"')),
        (D, bash("git show HEAD")),
        (D, bash("git --no-pager log -p -3")),
        (D, bash("echo $(git show HEAD)")),
        (D, bash("cd kalos-dex && git show")),
        (D, bash("cat > n.md <<EOF\n$(git show HEAD)\nEOF")),                       # unquoted heredoc: runs
        (A, bash("git show --stat HEAD")),
        (A, bash("git log -p -- kalos-dex/firmware/kalos-dex/src/pet.cpp")),
        (A, bash("git log -3 --oneline")),
    ]
    for want, data in scripts:
        why, _ = g.decide(data)
        got = D if why else A
        fails += got != want
        what = data["tool_input"]["command"].replace(R, "<R>").replace(tmp, "<tmp>").replace("\n", "⏎")
        print("%s script/heredoc %-5s %s%s" % ("PASS" if got == want else "FAIL", got, what[:90], "" if got == want else "   <- wanted %s (%s)" % (want, why)))
    for p in files:
        os.remove(p)
    json.dump({"hardware": "test"}, open(g.UNLOCK, "w"))
    extra = [(A, tool("Read", file_path=R + "/kalos-dex/scad/caps.scad")),
             (D, tool("Read", file_path=R + "/kalos-dex/_archive/README.md")),
             (A, bash("cat .agent/zones.unlock")),
             (D, bash("git push"))]                           # unlocking a zone never opens a hard gate
    for want, data in extra:
        why, _ = g.decide(data)
        got = D if why else A
        fails += got != want
        print("%s unlock(hardware) %-5s %s" % ("PASS" if got == want else "FAIL", got, data["tool_input"]))
    json.dump({"sealed": "test"}, open(g.UNLOCK, "w"))                # publishing: the user unlocked 'sealed'
    sealed = [(A, bash("python3 public-draft/build_public.py"))]
    for want, data in sealed:
        why, _ = g.decide(data)
        got = D if why else A
        fails += got != want
        print("%s unlock(sealed) %-5s %s%s" % ("PASS" if got == want else "FAIL", got, data["tool_input"], "" if got == want else "  (%s)" % why))
    extra += sealed
    os.remove(g.UNLOCK)
    g.TRACE = os.path.join(tmp, "trace.jsonl")                    # trace names which subagent made the call
    for aid in ("a176006a4d22", ""):
        g.trace(dict(bash("ls"), agent_type="general-purpose", agent_id=aid), "allow", None)
    ids = [json.loads(l)["agent_id"] for l in open(g.TRACE)]
    ok = ids == ["a176006a", ""]
    fails += not ok
    print("%s trace agent_id %s" % ("PASS" if ok else "FAIL", ids))
    total = len(CASES) + len(scripts) + len(extra) + 1
    print("\n%s: %d cases, %d failed" % ("ALL PASS" if not fails else "FAILED", total, fails))
    sys.exit(1 if fails else 0)

if __name__ == "__main__":
    main()

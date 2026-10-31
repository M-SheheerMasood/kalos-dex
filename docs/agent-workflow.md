# How I work with the AI agent (the harness)
The firmware, tools and OpenSCAD in this repo were written by an AI coding assistant (a command-line agent that can
read files, run commands and edit code). I designed the gadget, made the decisions, did all the hardware work and
tested every step on the device. This page is about the part in between: the rules and tooling I set up so the agent
works the way I want, and what changed once it was used for real work. The code is in `agent-harness/`.

## Why
For the first two weeks the rules lived in a text file the agent reads at the start (AGENTS.md here). That mostly
worked, but "mostly" isn't good enough when the agent can run any command on my PC: it could read old archived files
and get confused by outdated facts, burn context on folders that were finished, or run a git command I didn't ask for.
Rules in prose rely on the agent remembering and choosing to follow them.

So I paused firmware work until the workflow was **deterministic and traceable, enforced by hooks instead of trust**:
the same request gets the same answer from a script, every tool call is logged, and the important rules can't be
skipped because a program checks them before anything runs.

## What it does
The agent CLI can run a script before every tool call (a "pre-tool hook") and on events like the start of a session.
The harness is a few of those scripts plus a step tool:

| Piece | What it does |
|---|---|
| **Zones** (`zones.json`) | The project is split into zones. *Locked* zones (finished hardware, old archives, build output, the public repo folder, closed log volumes) can't be read, searched or edited. *Read-only* zones (the harness itself, reference docs) can be read but not changed. |
| **Zone guard** (`hooks/zone_guard.py`) | Runs before **every** tool call. Checks file paths, search roots and shell commands (including wildcards that could expand into a locked folder) against the zones, and denies with a reason. Before a script the agent wrote runs, its text gets the same check. Fails closed: if the guard itself breaks, tools are denied. 206 test cases. |
| **Unlocking** (`zone.sh`) | Only I can unlock a zone, by typing the command myself. The agent has to ask, say why, and wait. |
| **Hard gates** | Some commands are denied no matter what: git push / merge / pull / rebase / reset --hard / amend, deleting branches or tags, turning hooks off, editing the agent's settings. The agent prepares the exact line, and I run it. |
| **Trace** | Every tool call is logged (time, session, which agent or sub-agent, tool, target, allowed or denied, which rule). |
| **State** (`hooks/events.py`) | A short state file (current step, decisions, next action) is fed back to the agent at every start, resume and context compaction, so a long session doesn't lose track. A stop hook reminds it to update the state file when code changed after it. |
| **Step workflow** (`workflow-step-skill.md`) | Every task runs as plan, then questions, then output, then testing. The plan lists 2–3 options (always including "don't / smallest version"), the RAM/flash cost, and the test that will prove it. Nothing gets built before I answer. |
| **Step tool** (`step.py`) | `check` runs the guard tests, the hook tests, a freshness check of the code map, and both firmware builds, and remembers a fingerprint of the exact tree it checked. `done` refuses to commit if anything changed after the check, or if a new file isn't named. So only a tested tree gets committed, with the step id and what was tested in the commit message. |
| **Code map** (`tools/codemap.py`) | A generated map of the firmware (files, functions with line numbers, who calls whom), so the agent reads the map and then only the lines it needs instead of whole folders. |

## What I decided in the design
The harness was built in small steps, each one planned by the agent with options, pros and cons, and decided by me:
- **Hooks, not prose.** Workflow rules that matter are checked by a script. Firmware work stayed paused until that existed.
- **Locked zones only I can unlock**, and hard gates on everything that publishes or rewrites history: the agent can
  prepare a push, but it never runs one.
- **New files need my OK** before they're committed (the step tool stops and lists them).
- **"Always rebuild"** in the check: both builds run every time, even if it takes longer, since my time waiting matters
  less than a commit that doesn't build. Since the public copy (no private sprites) broke twice without anyone
  noticing, the check also builds and compiles that copy whenever firmware, tools or the published files changed.
- **The edit gate is soft** (the agent recommended this and I agreed): edits before my answer aren't blocked, but
  they're logged, and the step workflow says to wait.
- **The harness is experimental, so any issue comes to me first.** The agent may not "fix" the harness on its own.
- **3-strike rule for false blocks:** when the guard blocks something it shouldn't, the agent works around it the
  allowed way and adds it to a tally. Only when the same false block has happened 3 times does it bring me a fix,
  with pros, cons, how often it hit, and the unlock line. Then I decide.
- **Test before trusting it:** before using it for real work, I had it run 3 dry-run tasks written the way I
  normally prompt (a watch-face request, a bug report, a request that needed a locked folder). In all three it
  paused, asked, and didn't work around a block.
- **Plans state the cost for other Pokémon (added in v0.8.0).** The firmware will be built for other Pokémon lines
  later, so every plan that touches the pet, its art or its evolutions keeps line-specific choices as data with a
  generic fallback, and says what each new line, stage or evolution would need because of it (art, a config line,
  code) and what happens if that's missing. If something looks better now but adds work per line, the plan has to
  say so, and I decide.

## What changed once it was used for real
- **The first guard was too strict.** It treated any `*` anywhere in a command as a wildcard into locked folders,
  and it blocked *running* the Python environment just because its folder was sealed. Both were found in real use,
  proposed as fixes with pros and cons, and changed only after I unlocked the harness for them: now only unquoted
  words count as wildcards (the command is split the way the shell would split it), and the sealed environment's
  Python may run while its folder stays sealed. The guard's tests grew with every fix.
- **The 3-strike rule's first fix:** a locked folder named inside plain text (a note saved with a heredoc) kept
  tripping the guard. After it hit a 4th time I unlocked the harness for the fix: text that a command only *saves*
  (`cat > file <<'X'`) no longer counts as a path, but anything fed into Python or a shell still does, and so does a
  saved file that the same command then runs. The second one, a `*` inside quoted heredoc text read as a file wildcard (the shell never expands it there), reached 3 hits a day later and was fixed the same way.
- **A guard only sees the command, not what a script does.** While preparing this update, a render script the agent
  had written saved images into a locked folder: the guard checked `python3 script.py`, not where the script writes.
  The agent reported it to me before doing anything else. I asked how to stop that without blocking normal notes and
  files, and we added two layers: the guard now **reads any script the agent wrote before running it** and applies the
  same locked-path check to its text, and the step check **fails on any change inside a locked zone** until I unlock
  that zone or confirm it with a flag only I can use (the guard denies that flag to the agent). Before switching it on,
  every command from the day was replayed through the old and new guard: the only new blocks were real mentions of
  locked folders in scripts.
- **Logging which sub-agent did what** was added after the dry runs, when the trace couldn't tell them apart.
- **A script that ignored a flag.** The agent ran the public build script with `--help` to see its options; the
  script didn't know the flag, ignored it, and ran a real build into the local public copy. The locked-zone row of
  the step check caught it and I reverted it. Two changes: the script stops on any flag it doesn't know, and the
  guard only lets it build into a preview folder until I unlock the public copy. The agent's rule now: read a
  script's code to learn its options instead of running it.

## A real task with it: the UI polish (firmware 0.7.0)
I wanted the screens to look less blocky and more natural: the built-in fonts are 1 px wide strokes, and the bars were
thin square outlines. How the step went:
1. **Plan:** the agent read the code map, then only the drawing code. It offered 3 options: bars only (no flash cost),
   bars + one rounded smooth font in 2 sizes (recommended, ~35 KB of ~880 KB free flash, no new RAM buffers), or a full
   layout redesign (bigger, mixes taste changes in, later).
2. **Previews before code:** real-size PNG mock-ups of the Home and Settings screens in 3 free (SIL OFL) rounded fonts,
   and a 4x close-up of today's bar next to HP bars with a 2 px and a 3 px rim.
3. **My picks:** Nunito ExtraBold, the 2 px rim, and outlined "cards" for the Settings rows.
4. **Build:** a small font converter, a shared `ui` module (text, width fit, HP bar) and every screen except the clock
   switched over. Each label is checked against the safe width and drops to a smaller size if it would run off the edge.
5. **Testing on the gadget, at my desk angle:** every screen, feed/play messages, both Dex pages, Settings incl. the
   reset bar. RAM on the Home screen stayed the same (245 KB free, biggest block 107 KB); flash +41 KB.
6. **Follow-up found while testing:** the "g" of the pet's name sat 1 px above the end of the red title bar. Instead of
   guessing, the agent added a test screen to the hardware-test build with 9 variants (text moved up, 2–6 extra red
   rows) that I stepped through on the device. I picked a mix of two (text 3 px up + 2 rows), then nudged the text and
   the mute badge once more by eye. The agent checked what each extra row would push down on every screen first.

The display is a TN panel: from my desk angle colours wash out or darken. Before the UI step we ran a similar on-device
test card for gamma presets and the panel's VCOM setting; gamma changed nothing useful, the VCOM offset did, and I
picked the value by eye. The UI step then used deeper bar colours and strong outlines because of it.

<img width="240" alt="Render of the Settings screen" src="../media/render-settings.png" /> <img width="240" alt="Render of the Home screen with the placeholder pet" src="../media/render-home.png" />

Renders of the current layout with the real font (the public build shows the placeholder pet; my own build uses
sprites I don't redistribute). Mood bubbles I designed for the pet (sad, angry, tired):

<img width="380" alt="Mood bubbles: gloom lines, angry mark, low battery" src="../media/mood-bubbles.png" />

## What I'd keep for next time
- Decide the rules *before* the agent has full access, and make the important ones checkable by a script.
- Expect the first version of a guard to be wrong in practice. Keep a tally instead of reacting to every annoyance,
  and let real use decide what to change.
- Side-by-side tests on the real device beat previews for small visual choices: a PC preview can't show the case
  lip over the top of the screen or the panel's viewing angle.

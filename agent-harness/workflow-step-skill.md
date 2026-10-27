---
name: kalos-step
description: Use for EVERY task in the Kalos Dex repo that changes or designs something (firmware, tools, docs, screen/clock-face design, harness) or chases a bug. Runs plan -> questions -> output -> testing, with costs, alternatives and evidence. Trivial, fully specified edits take the quick path.
---

# Kalos step: plan → questions → output → testing
Ideas adapted from obra/superpowers (MIT): brainstorming, writing-plans, verification-before-completion,
systematic-debugging – cut down to what this project needs.

## 0. Size it first
- **Quick** – the request says exactly what to change and it touches ≤ 2 files, no new RAM/flash, no new UI
  (e.g. "night dimming ends 07:30"). Skip 1–2 and the step file: say the change in one line, do it, then 4
  (test + one session-log line + commit).
- **Bug** – something behaves wrong → use the *Debugging* rules below inside 1.
- **Normal** – everything else (feature, screen, clock face, protocol, tool, harness change).

## 1. Plan (no edits yet)
1. **Look before asking.** Read `kalos-dex/CONTEXT.md` §1 (if not already in context) and **say first if a pause or
   open decision there blocks or touches this task** (e.g. "firmware is paused"). Then read only the code the task
   touches – map first (`docs/codemap.md` when it exists), then `Read` with offset/limit, never whole folders.
   Never ask what the code or docs already answer.
2. **Echo the goal** in 1–2 sentences in the user's terms, plus what you found that changes the picture
   (a constraint, a conflict, a cheaper path).
3. **2–3 real approaches, always including "don't / smallest version"** when the task is optional. For each:
   what it trades off. **Recommend one and say why.** Drop options that fight the project rules.
4. **Costs, measured or estimated with the basis stated:**
   - RAM: static + peak while the screen is shown, against PLAN §8 (biggest block ~107 KB, no 16-bit full-screen
     buffer, sprites created in `enter()` and freed in `leave()`), flash vs the 1.4 MB app limit.
   - Layout: nothing important within `TOP_INSET` (12 px) of the top or 4 px of the other edges.
   - Art stays placeholder and modular (`pet_line.h` data, `pet_art.*` drawing) unless told otherwise.
   - Tokens/time when the work is big (e.g. a simulator).
5. **Design work** (clock faces, screen layouts, anything the user will look at): show, don't describe –
   offer a PNG preview (`.scratch/venv/bin/python` + Pillow, e.g. `tools/clock_preview.py`) or a published page with
   2–3 variants side by side at real size (240x320) before writing firmware.
6. **Test that will prove it** – name the exact commands / device checks (see 4).
7. Write the step file `.agent/state/steps/<YYYY-MM-DD-NN>-<slug>.md`: goal · approaches + choice · costs ·
   files to touch · test · (later) result + commit.

## 2. Questions
- Ask **2–4 specific questions max**, each saying what the answer changes. Prefer "I propose X because Y – OK?"
  over open lists. Electronics/safety warnings go BEFORE any step they apply to.
- Then **stop and wait.** The user's reply is the approval; don't edit before it (soft gate, logged).

## 3. Output
- Only what the plan says. No "while I'm here" changes; anything new → note it as a follow-up.
- Follow the code around you (naming, comment density, idiom). Test helpers only in the `hwtest` build.
- Firmware: one feature per step, small enough to test on its own.

## 4. Testing – evidence before claims
Never say done / fixed / works / passes without running the check **in this step** and reading its output.
| Claim | Evidence needed |
|---|---|
| builds | `pio run -e esp32dev` **and** `-e hwtest`: both exit 0 (sizes noted) |
| works on the device | upload, boot log read (`serial_monitor.py`), the feature exercised (`serial_send.py` / user) |
| memory OK | the "RAM free / biggest block" lines on entering + leaving the screen, vs PLAN §8 |
| looks right | a preview image or the user's eyes – say which |
| harness OK | `python3 .agent/hooks/test_zone_guard.py` ALL PASS (+ new cases for new rules) |
| fixed | the original symptom reproduced before, gone after |
If something can't be tested here (needs the user's eyes or hands), say exactly what they should check.
Then: CONTEXT/PLAN/CHANGELOG only if their facts changed (a richer session-log entry is fine too) →
`python3 kalos-dex/tools/step.py check` (ALL PASS) → `step.py done <step id> "what changed"` (step-file result,
session-log line, one commit). New files → it stops and lists them: ask the user, then `--add`. Report with the evidence.

## Debugging (inside step 1)
1. **Root cause before any fix.** Read the full error / serial log; reproduce it reliably; check what changed
   (`git log -p` on the touched files, with a path).
2. One written hypothesis ("X because Y"), one smallest test of it, one variable at a time.
3. Fix at the source, then prove it with the original symptom.
4. **3 failed fixes → stop**, tell the user, question the design instead of trying a 4th.

## Hard stops from hooks
If a hook denies something (locked zone, git push/merge, harness edit): don't work around it. Tell the user
what you were doing and why it was blocked. The harness is experimental: report any issue to the user before
trying a fix nobody authorized. A block you think is wrong goes into the "guard misses" tally in `STATE.md`;
only at **3+ hits of the same kind** propose the exact change (pros, cons, how often, unlock line) – the user decides.

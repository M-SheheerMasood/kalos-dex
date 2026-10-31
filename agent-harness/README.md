# agent-harness
The hooks and tools that keep the AI coding agent inside the rules on this project. What it is and why:
`docs/agent-workflow.md`.

**A copy for reading, not a drop-in package.** In my working repo these files live in the agent's own config folder
and are wired up through its settings file; the paths in `zones.json` and the tests refer to that private layout
(this public repo is flattened and leaves out the private docs). Folder and tool names were made generic for this copy.
It's written for an agent CLI that can run a command before every tool call and on session events, passing the call as
JSON on stdin.

| File | Role |
|---|---|
| `hooks/zone_guard.py` | pre-tool hook: zones, wildcard/shell checks, hard gates, trace log |
| `hooks/test_zone_guard.py` | 206 cases: allowed / denied, with the reason |
| `hooks/events.py` (+ `test_events.py`) | session start/resume/compaction state feed, stop reminder, file-size warning |
| `hooks.json` | which event runs which script |
| `zones.json` | the zones: locked, read-only, active search roots, allowed commands |
| `zone.sh` | lock / unlock zones – run by me, never by the agent |
| `step.py` | `check` (no changes inside locked zones, tests, code map, both builds, the public copy compiled without the private art; remembers the tree) and `done` (commits only that tree) |
| `workflow-step-skill.md` | the step workflow the agent follows: plan, questions, output, testing |

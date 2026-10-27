#!/usr/bin/env bash
# Lock / unlock agent zones (see .agent/zones.json). Only YOU run this, in the Agent prompt with "!":
#   ! .agent/zone.sh status              which zones exist, which are unlocked
#   ! .agent/zone.sh unlock hardware     let the agent read/edit the hardware zone again
#   ! .agent/zone.sh lock hardware       lock it again       (! .agent/zone.sh lock all = lock everything)
# The agent can't run unlock/lock itself (the zone guard blocks it). Every change is written to the trace log.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 - "$DIR" "$@" <<'EOF'
import json, os, sys, time
d, args = sys.argv[1], sys.argv[2:]
zones = json.load(open(os.path.join(d, "zones.json"), encoding="utf-8"))
names = list(zones["locked"]) + list(zones["readonly"])
uf = os.path.join(d, "zones.unlock")
un = json.load(open(uf, encoding="utf-8")) if os.path.exists(uf) else {}
cmd = args[0] if args else "status"
if cmd in ("unlock", "lock"):
    if len(args) < 2 or (args[1] not in names and not (cmd == "lock" and args[1] == "all")):
        sys.exit("usage: zone.sh %s <%s>" % (cmd, "|".join(names + (["all"] if cmd == "lock" else []))))
    now = time.strftime("%Y-%m-%dT%H:%M:%S")
    if cmd == "unlock":
        un[args[1]] = now
    elif args[1] == "all":
        un = {}
    else:
        un.pop(args[1], None)
    if un:
        json.dump(un, open(uf, "w", encoding="utf-8"), indent=1)
    elif os.path.exists(uf):
        os.remove(uf)
    os.makedirs(os.path.join(d, "logs"), exist_ok=True)
    with open(os.path.join(d, "logs", "trace.jsonl"), "a", encoding="utf-8") as f:
        f.write(json.dumps({"t": now, "session": "user", "tool": "zone.sh", "target": " ".join(args),
                            "decision": cmd, "why": "by the user"}) + "\n")
elif cmd != "status":
    sys.exit("usage: zone.sh status | unlock <zone> | lock <zone|all>")
for kind in ("locked", "readonly"):
    for n, v in zones[kind].items():
        state = ("UNLOCKED since " + un[n]) if n in un else ("locked" if kind == "locked" else "read-only")
        print("%-9s %-24s %s" % (n, state, v["why"]))
EOF

#!/usr/bin/env python3
"""Code map of the firmware, generated from the source – never edit the outputs by hand.

usage (WSL python3, standard library only, from anywhere):
  python3 kalos-dex/tools/codemap.py           -> docs/codemap.md   (for the agent: files, functions with line ranges,
                                                   who calls whom, boot + loop order, screens + buttons)
  python3 kalos-dex/tools/codemap.py --html    -> also docs/codemap.html (for the user: 3 diagrams, open with Live Server)
  python3 kalos-dex/tools/codemap.py --check   -> exit 1 if docs/codemap.md is out of date (used by step checks)
Deterministic: same source -> byte-identical output (sorted, no timestamps; a hash of the source is in the header).
Reads only firmware/kalos-dex/src + include. Line numbers are 1-based, as in the editor.
"""
import hashlib, html, os, re, sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
KD = os.path.dirname(TOOLS)
FW = os.path.join(KD, "firmware", "kalos-dex")
OUT_MD = os.path.join(KD, "docs", "codemap.md")
OUT_HTML = os.path.join(KD, "docs", "codemap.html")
MERMAID = "https://cdn.jsdelivr.net/npm/mermaid@10.9.1/dist/mermaid.min.js"

# which layer each module belongs to (module = file name without .h/.cpp); unknown modules go to "other"
LAYERS = [
    ("core", ["main", "config", "system", "storage", "timekeeper"]),
    ("services", ["pet", "media", "night", "pclink"]),
    ("hardware I/O", ["input", "display", "leds"]),
    ("screens + drawing", ["manager", "screen", "boot", "home_screen", "dex_screen", "clock_screen", "music_screen",
                           "settings_screen", "pet_art", "pet_line"]),
    ("test build only", ["test_screens"]),
]
CONTROL = {"if", "for", "while", "switch", "catch", "return", "sizeof", "else", "do", "defined"}
SKIP_CALLS = {"Serial", "ESP", "delay", "snprintf", "printf", "sizeof", "millis", "heap_caps_get_largest_free_block"}


# ---------------- reading ----------------
def sources():
    files = []
    for sub in ("include", "src"):
        for dp, dns, fns in os.walk(os.path.join(FW, sub)):
            dns.sort()
            for fn in sorted(fns):
                if fn.endswith((".h", ".cpp")):
                    files.append(os.path.relpath(os.path.join(dp, fn), FW).replace(os.sep, "/"))
    return sorted(files)


def module_of(path):
    return os.path.splitext(os.path.basename(path))[0]


def strip(code):
    """Comments, strings, chars and preprocessor lines -> spaces (newlines kept, so line numbers stay right)."""
    out, i, n = [], 0, len(code)
    while i < n:
        c, two = code[i], code[i:i + 2]
        if two == "//":
            j = code.find("\n", i); j = n if j < 0 else j
            out.append(" " * (j - i)); i = j
        elif two == "/*":
            j = code.find("*/", i + 2); j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", code[i:j])); i = j
        elif c in "\"'":
            j = i + 1
            while j < n and code[j] != c:
                j += 2 if code[j] == "\\" else 1
            out.append(c + " " * (j - i - 1) + c if j < n else ""); i = j + 1
        elif c == "#" and (i == 0 or code[i - 1] == "\n" or code[:i].rsplit("\n", 1)[-1].strip() == ""):
            j = code.find("\n", i); j = n if j < 0 else j
            while j < n and code[j - 1] == "\\":
                j = code.find("\n", j + 1); j = n if j < 0 else j
            out.append(re.sub(r"[^\n]", " ", code[i:j])); i = j
        else:
            out.append(c); i += 1
    return "".join(out)


def purpose(raw):
    """First comment line of the file = what it is for."""
    for line in raw.splitlines():
        s = line.strip()
        if s.startswith("//"):
            return s.lstrip("/ ").strip()
        if s and not s.startswith("#"):
            break
    return ""


FUNC_RE = re.compile(r"([~A-Za-z_][\w:]*)\s*\((?:[^()]|\([^()]*\))*\)\s*(?:const\s*)?(?:override\s*)?(?:noexcept\s*)?$")


def functions(code):
    """[(qualified name, start line, end line)] for every function body at namespace / class / file level."""
    found, stack, depth, start = [], [], 0, 0          # stack: (kind, name, depth at open, start line)
    lineno = lambda pos: code.count("\n", 0, pos) + 1
    for pos, ch in enumerate(code):
        if ch in ";{}":
            if ch == "{":
                in_func = any(k == "func" for k, *_ in stack)
                head = code[start:pos].strip()
                kind, name, at = "other", "", start
                if not in_func:
                    m_ns = re.search(r"\bnamespace\s*(\w*)\s*$", head)
                    m_cl = re.search(r"\b(class|struct|union)\s+(\w+)[^()]*$", head)
                    m_fn = FUNC_RE.search(head)
                    if m_ns:
                        kind, name = "ns", m_ns.group(1)
                    elif re.search(r"\benum\b", head):
                        kind = "other"
                    elif m_cl and "=" not in head:
                        kind, name = "class", m_cl.group(2)
                    elif m_fn and m_fn.group(1).split("::")[-1] not in CONTROL and "=" not in head[:m_fn.start()]:
                        kind, name = "func", m_fn.group(1)
                        at = start + m_fn.start() + (len(code[start:pos]) - len(code[start:pos].lstrip()))
                stack.append((kind, name, depth, lineno(at)))
                depth += 1
            elif ch == "}" and stack:
                depth -= 1
                kind, name, _, line = stack.pop()
                if kind == "func":
                    scope = [nm for k, nm, *_ in stack if k in ("ns", "class") and nm]
                    q = "::".join(scope + [name]) if not name.startswith(tuple(s + "::" for s in scope)) else name
                    found.append((q, line, lineno(pos)))
            start = pos + 1
    return sorted(found, key=lambda f: f[1])


def body_lines(raw_lines, start, end):
    return [(i, raw_lines[i - 1]) for i in range(start, end + 1)]


# ---------------- analysis ----------------
def analyse():
    files = sources()
    data = {"files": [], "funcs": {}, "ns_owner": {}, "edges": {}, "includes": {}}
    h = hashlib.sha1()
    raws = {}
    for f in files:
        raw = open(os.path.join(FW, f), encoding="utf-8", errors="replace").read()
        raws[f] = raw
        h.update(f.encode() + b"\0" + raw.encode())
        code = strip(raw)
        data["files"].append((f, raw.count("\n") + (0 if raw.endswith("\n") else 1), purpose(raw)))
        data["funcs"][f] = functions(code)
        data["includes"][f] = sorted(set(module_of(i) for i in re.findall(r'#include\s+"([^"]+)"', raw)))
    # a namespace belongs to the module whose .cpp defines it (screens:: -> manager.cpp), else to its header
    for want_cpp in (False, True):
        for f in files:
            if f.endswith(".cpp") == want_cpp:
                for ns in re.findall(r"\bnamespace\s+(\w+)\s*\{", strip(raws[f])):
                    data["ns_owner"][ns] = module_of(f)
    data["hash"] = h.hexdigest()[:12]
    data["raws"] = raws
    # calls between modules: ns::function( in one module -> the module that owns ns
    for f in files:
        code = strip(raws[f])
        me = module_of(f)
        for ns, fn in re.findall(r"\b([A-Za-z_]\w*)::(\w+)\s*\(", code):
            owner = data["ns_owner"].get(ns)
            if owner and owner != me:
                data["edges"].setdefault((me, owner), set()).add(fn)
    return data


def calls_in(raw_lines, start, end):
    """Ordered calls inside a function body: [(line, call, comment)] (skips logging/delays)."""
    out = []
    for i, line in body_lines(raw_lines, start + 1, end - 1):
        code, _, comment = line.partition("//")
        s = code.strip()
        if not s or s.startswith(("Serial", "tft", "delay", "for", "#")) or s.split("(")[0].strip() in SKIP_CALLS:
            continue
        names = [m.group(1) for m in re.finditer(r"(?<![.\w])([A-Za-z_]\w*(?:::\w+)?)\s*\(", code)
                 if m.group(1) not in CONTROL and m.group(1).split("::")[0] not in SKIP_CALLS]
        if not names:
            continue
        if re.match(r"(if|while)\b", s):              # if (a()) b();  /  while (a(e)) b(e);
            label = "%s %s → %s" % (s.split()[0].split("(")[0], names[0], names[1]) if len(names) > 1 else "if " + names[0]
        elif s.startswith("else"):
            label = "else → " + names[0]
        else:
            label = names[0]                           # the outer call; calls inside its arguments are details
        out.append((i, label, comment.strip()))
    return out


def screens_info(data):
    raw = data["raws"]["src/screens/manager.cpp"].splitlines()
    orders, build = {}, "gadget"
    for line in raw:
        s = line.strip()
        if s.startswith("#ifdef HWTEST"):
            build = "hwtest"
        elif s.startswith("#else") and build == "hwtest":
            build = "gadget"
        elif s.startswith("#endif"):
            build = "gadget"
        m = re.search(r"static Screen \*order\[\] = \{([^}]*)\}", line)
        if m:
            orders[build] = [x.strip().lstrip("&").replace("Screen", "") for x in m.group(1).split(",")]
    buttons = []                                              # (screen, event, button, action, file:line)
    for f, funcs in data["funcs"].items():
        for name, s, e in funcs:
            if not (name.endswith("::onEvent") or name == "screens::handle"):
                continue
            who = "everywhere" if name == "screens::handle" else name.split("::")[-2].replace("Screen", "").lower()
            lines = raw_f = data["raws"][f].splitlines()
            default_ev = None
            for i, line in body_lines(raw_f, s, e):
                code = line.split("//")[0]
                g = re.search(r"e\.type\s*!=\s*Ev::(\w+)", code)
                if g and "return false" in code and "Btn::" not in code:
                    default_ev = g.group(1)
                btns = re.findall(r"e\.button\s*==\s*Btn::(\w+)", code)
                evs = re.findall(r"e\.type\s*==\s*Ev::(\w+)", code)
                if not btns and evs == ["Turn"]:
                    btns = ["Knob"]
                if not btns and not evs:
                    continue
                ev = evs[0] if evs else (default_ev or "Press")
                acts = [c for c in re.findall(r"\b([A-Za-z_]\w*(?:::\w+)?)\s*\(", code)
                        if c not in CONTROL and c not in ("isDown", "snprintf") and not c.startswith("input::")]
                asg = re.search(r"^\s*(\w+)\s*=", code)
                act = acts[0] if acts else (asg.group(1) + " changes" if asg else (line.split("//")[1].strip() if "//" in line else "see code"))
                if "?" in code and len(btns) == 1 and not acts:
                    btns = ["Feed/Play"]
                for b in btns:
                    buttons.append((who, ev, b, act, "%s:%d" % (f, i)))
    # a vague "see code" row is dropped when the same screen + button has a clearer row
    clear = {(w, e, part) for w, e, b, a, _ in buttons if a != "see code" for part in b.split("/")}
    seen, kept = set(), []
    for r in buttons:
        key = (r[0], r[1], r[2])
        if r[3] == "see code" and (key in clear or key in seen):
            continue
        seen.add(key)
        kept.append(r)
    buttons = kept
    return orders, buttons


# ---------------- outputs ----------------
def layer_of(mod):
    for name, mods in LAYERS:
        if mod in mods:
            return name
    return "other"


def write_md(data, orders, buttons, boot, loop):
    L = ["# Firmware code map – GENERATED by `tools/codemap.py`, do not edit (source hash %s)" % data["hash"],
         "Read this first, then open only the lines you need (`Read` with offset/limit). Line numbers are 1-based.",
         "Regenerate: `python3 kalos-dex/tools/codemap.py` (`--html` also writes the diagrams page, `--check` = stale?).", ""]
    L += ["## Files (%d, %d lines)" % (len(data["files"]), sum(n for _, n, _ in data["files"])),
          "| file | lines | layer | purpose |", "|---|---|---|---|"]
    for f, n, p in data["files"]:
        L.append("| `%s` | %d | %s | %s |" % (f, n, layer_of(module_of(f)), p.replace("|", "/")[:110]))
    L += ["", "## Who calls whom (module → module: functions used)"]
    for (a, b), fns in sorted(data["edges"].items()):
        L.append("- %s → %s: %s" % (a, b, ", ".join(sorted(fns))))
    L += ["", "## Boot order (`main.cpp` setup)"] + ["- L%d `%s`%s" % (i, c, (" – " + cm) if cm else "") for i, c, cm in boot]
    L += ["", "## Loop order (`main.cpp` loop, every ~5 ms)"] + ["- L%d `%s`%s" % (i, c, (" – " + cm) if cm else "") for i, c, cm in loop]
    L += ["", "## Screens (Back = next, hold Back = Home)"]
    for b in sorted(orders):
        L.append("- %s build: %s" % (b, " → ".join(orders[b])))
    L += ["", "## Buttons (derived from the code – check the line)", "| where | event | button | does | at |", "|---|---|---|---|---|"]
    for who, ev, b, act, at in buttons:
        L.append("| %s | %s | %s | `%s` | %s |" % (who, ev, b, act, at))
    L += ["", "## Functions by file (name: start–end)"]
    for f, _, _ in data["files"]:
        fs = data["funcs"][f]
        if fs:
            L.append("- **%s**: " % f + "; ".join("%s %d–%d" % (n, s, e) for n, s, e in fs))
    return "\n".join(L) + "\n"


def mid(s):
    return re.sub(r"\W", "_", s)


def write_html(data, orders, buttons, boot, loop):
    # 1. module map: layers top -> bottom; main (starts/updates everything, see diagram 2) and the test build left out
    LEAVE_OUT = {"main", "test_screens", "config", "screen"}
    ROWS = [("screens + drawing", "what you see"), ("services", "the gadget's logic"),
            ("core", "time, saving, safety"), ("hardware I/O", "screen, buttons, lights")]
    m1 = ["flowchart TB"]
    mods = sorted(set(module_of(f) for f, _, _ in data["files"]) - LEAVE_OUT)
    for name, sub in ROWS:
        inside = [m for m in mods if layer_of(m) == name]
        if inside:
            m1.append('  subgraph %s["%s – %s"]' % (mid(name), name, sub))
            m1.append("    direction LR")
            m1 += ['    %s["%s"]' % (mid(m), m) for m in inside]
            m1.append("  end")
    for (a, b), fns in sorted(data["edges"].items()):
        if a not in LEAVE_OUT and b not in LEAVE_OUT:
            m1.append('  %s --> %s' % (mid(a), mid(b)))
    # 2. boot + loop: two separate top-to-bottom diagrams, shown side by side
    def chain(steps, prefix, back):
        out, ids = ["flowchart TB"], []
        for k, (i, c, cm) in enumerate(steps):
            ids.append("%s%d" % (prefix, k))
            out.append('  %s["L%d · %s"]' % (ids[-1], i, html.escape(c)))
        out += ["  %s --> %s" % (a, b) for a, b in zip(ids, ids[1:])]
        if back and ids:
            out.append("  %s -.->|next loop| %s" % (ids[-1], ids[0]))
        return out
    m2a, m2b = chain(boot, "b", False), chain(loop, "l", True)

    # 3. screens + buttons: one left-to-right ring per build (gadget shown, test build folded away)
    def ring(build):
        seq, out = orders[build], ["flowchart LR"]
        for s in seq:
            acts = ["%s %s: %s" % (bt, ev.lower(), act) for who, ev, bt, act, _ in buttons if who == s.lower()]
            label = "<b>%s</b>" % s + "".join("<br/><small>%s</small>" % html.escape(a) for a in acts)
            out.append('  %s["%s"]' % (mid(s), label))
        out += ["  %s --> %s" % (mid(x), mid(y)) for x, y in zip(seq, seq[1:])]
        out.append('  %s -.->|Back| %s' % (mid(seq[-1]), mid(seq[0])))
        return out
    m3 = ring("gadget")
    glob = "<br/>".join(html.escape("%s %s: %s" % (bt, ev.lower(), act)) for who, ev, bt, act, _ in buttons if who == "everywhere")
    m3.append('  everywhere["<b>on every screen</b> (unless the screen uses it)<br/><small>%s</small>"]' % glob)
    m3t = ring("hwtest") if "hwtest" in orders else ["flowchart LR", "  none"]
    table = "".join("<tr><td>%s</td><td>%d</td><td>%s</td><td>%s</td></tr>" % (
        html.escape(f), n, html.escape(layer_of(module_of(f))),
        "<br>".join("%s <span class=ln>%d–%d</span>" % (html.escape(nm), s, e) for nm, s, e in data["funcs"][f]))
        for f, n, _ in data["files"])
    page = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kalos Dex code map</title>
<style>
:root{--bg:#fbfaf8;--fg:#1d1b19;--muted:#6b645c;--line:#e4dfd8;--accent:#c8102e}
@media (prefers-color-scheme:dark){:root{--bg:#161412;--fg:#ece7e1;--muted:#a39b91;--line:#34302b;--accent:#ff5a6e}}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,sans-serif}
main{max-width:1200px;margin:auto;padding:24px max(16px,env(safe-area-inset-left))}
h1{font-size:22px;margin:0 0 4px}h2{font-size:17px;margin:32px 0 8px;border-bottom:2px solid var(--accent);display:inline-block}
p.note{color:var(--muted);margin:0 0 8px}.diagram{overflow-x:auto;border:1px solid var(--line);border-radius:8px;padding:12px}
table{border-collapse:collapse;width:100%%;font-size:13px}td,th{border-top:1px solid var(--line);padding:6px;vertical-align:top;text-align:left}
.ln{color:var(--muted);font-variant-numeric:tabular-nums}.two{display:grid;grid-template-columns:1fr 1fr;gap:16px}@media (max-width:700px){.two{grid-template-columns:1fr}}details{margin-top:24px}
</style></head><body><main>
<h1>Kalos Dex firmware – code map</h1>
<p class="note">Generated from the source by <code>tools/codemap.py --html</code> (source hash %s, %d files, %d lines). <b>L</b> = line in the file. Comments and exact functions: <code>docs/codemap.md</code>.</p>
<h2>1 · Modules – who calls whom</h2>
<p class="note"><b>Click a module</b> to see only its arrows (click it again for all). Arrow = "uses functions of". Not shown: <code>main</code> (starts and updates every module – diagram 2) and the test-build screens. Exact functions per arrow: codemap.md.</p>
<div class="diagram" id="modmap"><pre class="mermaid">%s</pre></div>
<h2>2 · Boot and loop</h2><p class="note">Left: what runs once at power-on, in order. Right: what runs over and over (~200 times a second).</p>
<div class="two"><div class="diagram"><b>setup()</b><pre class="mermaid">%s</pre></div><div class="diagram"><b>loop()</b><pre class="mermaid">%s</pre></div></div>
<h2>3 · Screens and buttons</h2><p class="note">Back button = next screen, hold Back = Home. Derived from each screen's <code>onEvent</code> – the code is the truth, the line is in codemap.md.</p>
<div class="diagram"><pre class="mermaid">%s</pre></div>
<details><summary>Test build (hwtest) screen order</summary><div class="diagram"><pre class="mermaid">%s</pre></div></details>
<details><summary><b>All functions with line ranges</b></summary><table><tr><th>file</th><th>lines</th><th>layer</th><th>functions</th></tr>%s</table></details>
</main>
<script src="%s"></script>
<script>
mermaid.initialize({startOnLoad:false, securityLevel:"loose", theme: matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "default", flowchart:{htmlLabels:true}});
mermaid.run().then(() => {                        // click a module in diagram 1 -> only its arrows stay bright
  const svg = document.querySelector("#modmap svg"); if (!svg) return;
  let on = null;
  const idOf = n => (n.id.match(/^flowchart-(.+)-\\d+$/) || [])[1];
  svg.querySelectorAll(".node").forEach(n => { n.style.cursor = "pointer"; n.addEventListener("click", () => {
    const id = idOf(n); on = on === id ? null : id;
    svg.querySelectorAll(".edgePaths path").forEach(p => {
      const hit = !on || p.classList.contains("LS-" + on) || p.classList.contains("LE-" + on);
      p.style.opacity = hit ? 1 : 0.07; p.style.strokeWidth = on && hit ? "2.5px" : "";
    });
    svg.querySelectorAll(".node").forEach(m => {
      const linked = !on || idOf(m) === on || [...svg.querySelectorAll(".edgePaths path")].some(p =>
        (p.classList.contains("LS-" + on) && p.classList.contains("LE-" + idOf(m))) ||
        (p.classList.contains("LE-" + on) && p.classList.contains("LS-" + idOf(m))));
      m.style.opacity = linked ? 1 : 0.25;
    });
  }); });
});
</script>
</body></html>
""" % (data["hash"], len(data["files"]), sum(n for _, n, _ in data["files"]),
       "\n".join(m1), "\n".join(m2a), "\n".join(m2b), "\n".join(m3), "\n".join(m3t), table, MERMAID)
    return page


def main():
    data = analyse()
    orders, buttons = screens_info(data)
    main_raw = data["raws"]["src/main.cpp"].splitlines()
    fmain = {n: (s, e) for n, s, e in data["funcs"]["src/main.cpp"]}
    boot = calls_in(main_raw, *fmain["setup"])
    loop = calls_in(main_raw, *fmain["loop"])
    md = write_md(data, orders, buttons, boot, loop)
    if "--check" in sys.argv:
        old = open(OUT_MD, encoding="utf-8").read() if os.path.exists(OUT_MD) else ""
        if old != md:
            print("codemap STALE – run: python3 kalos-dex/tools/codemap.py"); sys.exit(1)
        print("codemap fresh (%s)" % data["hash"]); return
    open(OUT_MD, "w", encoding="utf-8", newline="\n").write(md)
    print("wrote %s (%d lines, %d functions, hash %s)" % (os.path.relpath(OUT_MD, KD), md.count("\n"),
          sum(len(v) for v in data["funcs"].values()), data["hash"]))
    if "--html" in sys.argv:
        open(OUT_HTML, "w", encoding="utf-8", newline="\n").write(write_html(data, orders, buttons, boot, loop))
        print("wrote %s" % os.path.relpath(OUT_HTML, KD))


if __name__ == "__main__":
    main()

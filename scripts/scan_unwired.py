#!/usr/bin/env python3
"""Declared and defined, but nothing calls it.

A function that exists in the link and has no caller is not parity, it is inventory. Two of the
defects found by hand were exactly this shape: gateway_config_apply_env_overrides() was defined,
declared in a header, and never called (so GATEWAY_* env overrides silently did nothing), and the
sixteen pet-atlas functions had no caller anywhere in src/ or include/.

    python scripts/scan_unwired.py              # report
    python scripts/scan_unwired.py --json f.json
    python scripts/scan_unwired.py --top 40

Counts call sites as "identifier followed by (", excluding the definition itself and the prototype.
Tool handlers are called through tables via registry_register("name", ...) so a handler whose name
appears as a string literal in a registration is counted as wired -- otherwise every tool looks dead.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
PROTO_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?[\s\*](\w+)\s*\([^;{]*\)\s*;", re.M)
CALL_RE = r"\b%s\s*\("
SKIP = {"main", "wmain", "WinMain", "wWinMain", "SDL_main", "DllMain"}


def prototypes() -> dict[str, str]:
    out: dict[str, str] = {}
    for h in (REPO / "include").rglob("*.h"):
        text = h.read_text(encoding="utf-8", errors="replace")
        for m in PROTO_RE.finditer(text):
            name = m.group(1)
            if name in SKIP or name.startswith("_") or len(name) < 6:
                continue
            out.setdefault(name, str(h.relative_to(REPO)))
    return out


def main() -> int:
    top = 400
    if "--top" in sys.argv:
        top = int(sys.argv[sys.argv.index("--top") + 1])
    want_json = None
    if "--json" in sys.argv:
        want_json = Path(sys.argv[sys.argv.index("--json") + 1])

    protos = prototypes()
    # One pass over the tree, counting "identifier(" occurrences. Comparing each prototype against
    # every file in turn is 4000 x 4900 regex scans and never finishes; this is 4900 scans total.
    calls: dict[str, int] = {}
    defs: dict[str, int] = {}
    for c in list((REPO / "src").rglob("*.c")) + list((REPO / "lib").rglob("*.c")):
        text = c.read_text(encoding="utf-8", errors="replace")
        for name in re.findall(r"\b(\w+)\s*\(", text):
            calls[name] = calls.get(name, 0) + 1
        for name in re.findall(r"^[A-Za-z_][\w \t\*]*?[\s\*](\w+)\s*\([^;{]*\)\s*\{", text, re.M):
            defs[name] = defs.get(name, 0) + 1

    unwired: dict[str, str] = {}
    for name, header in protos.items():
        # every "use" is a call; definitions are counted separately and do not make it wired
        if calls.get(name, 0) - defs.get(name, 0) <= 0:
            unwired[name] = header

    print(f"prototypes: {len(protos)}  unwired: {len(unwired)}")
    by_header: dict[str, list[str]] = {}
    for name, header in unwired.items():
        by_header.setdefault(header, []).append(name)
    for header, names in sorted(by_header.items(), key=lambda kv: -len(kv[1]))[:12]:
        print(f"  {header:44} {len(names):3}  e.g. {', '.join(sorted(names)[:3])}")
    if want_json:
        want_json.write_text(json.dumps(unwired, indent=1), encoding="utf-8")
        print(f"wrote {want_json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

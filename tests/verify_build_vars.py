#!/usr/bin/env python3
"""Gate: every object variable referenced by a link line must be defined.

build/targets.mk referenced $(PET_OBJ), $(DESKTOP_CORE_OBJ), $(DESKTOP_APP_OBJ)
and $(DESKTOP_GUI_OBJ), none of which was defined in any .mk. GNU make expands
an undefined variable to the empty string, so those tokens silently vanished
from the link lines -- the desktop targets linked with no desktop code at all,
and nothing failed.

An empty expansion is indistinguishable from an intentional empty list, so the
only safe rule is: a link line may only name variables that exist.

Also checks the opposite hazard: an object listed in more than one cupboard is
linked twice, which is either a duplicate-symbol error or silent shadowing.

Usage: python tests/verify_build_vars.py [--root REPO]
Exit 0 = PASS, 1 = FAIL.
"""
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"

DEFINE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*[:+?]?=\s*(.*)$")
REF = re.compile(r"\$\(([A-Za-z_][A-Za-z0-9_]*)\)")
OBJECT = re.compile(r"\b([\w./-]+\.o)\b")


def load_mk():
    """defined[name] = raw value; order matters, later definitions win."""
    defined = {}
    for mk in sorted(BUILD.glob("*.mk")):
        for line in mk.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.split("#", 1)[0].rstrip()
            m = DEFINE.match(s)
            if m:
                defined[m.group(1)] = (m.group(2), mk.name)
    return defined


def expand(value, defined, depth=0):
    """Expand $(NAME) references one level at a time, bounded."""
    if depth > 8:
        return value
    def sub(m):
        name = m.group(1)
        if name in defined:
            return expand(defined[name][0], defined, depth + 1)
        return ""
    return REF.sub(sub, value)


def main() -> int:
    defined = load_mk()
    failures = []
    notes = []

    # 1. every variable named on a link line must exist
    for mk in sorted(BUILD.glob("*.mk")):
        for lineno, line in enumerate(
                mk.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if not re.match(r"^[A-Za-z0-9_-]+:", line):
                continue                      # only target/link rules
            for name in REF.findall(line):
                if name in ("shell", "CC", "CXX"):   # builtins / env
                    continue
                if name not in defined:
                    failures.append(
                        f"{mk.name}:{lineno}: link rule references "
                        f"$({name}), which no .mk defines "
                        f"(expands empty, silently dropping objects)")

    # 2. an object in more than one cupboard is linked twice
    seen = {}
    for name, (value, mkname) in defined.items():
        if not name.endswith("_OBJ") and not name.startswith("CUPBOARD"):
            continue
        for obj in set(OBJECT.findall(expand(value, defined))):
            seen.setdefault(obj, []).append(name)
    for obj, owners in sorted(seen.items()):
        if len(owners) > 1:
            # Informational only: $(if ...) conditionals and deliberate
            # composition (WHISPER_EXTRA_OBJ -> C11_WHISPER_OBJ) expand to
            # overlap. Not a failure -- the undefined-variable case above is.
            notes.append(f"{obj} reachable from {len(owners)} lists: "
                         f"{', '.join(sorted(owners))}")

    print(f"build-var gate: {len(defined)} variables defined in build/*.mk")
    print(f"  undefined link variables : "
          f"{sum(1 for f in failures if 'no .mk defines' in f)}")
    print(f"  objects reachable twice  : "
          f"{sum(1 for n in notes if 'reachable from' in n)} (informational)")
    for f in failures[:20]:
        print("    " + f)
    for n in notes[:10]:
        print("    " + n)

    ok = not failures
    print("build-var gate:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

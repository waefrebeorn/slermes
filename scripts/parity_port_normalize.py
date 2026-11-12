#!/usr/bin/env python3
"""parity_port_normalize.py — one command that keeps a port's registration
consistent.

A port touches four places, and getting any of them wrong is silent:
  src/<area>/port_<name>.c   the implementation (found by basename)
  include/port_<name>.h      the header
  build/objects.mk           the object must be listed
  tests/run_all_oracles.sh   the DEPS entry must name every object the
                            harness link needs

The DEPS line is derived from what the port actually includes, so it cannot
drift from the code:

  hermes_json.h      -> lib/libjson/json.o
  hash.h             -> lib/libhash/hash.o
  hermes_regex.h     -> lib/libregex/hermes_regex.o
  crypto.h           -> lib/libcrypto/crypto.o
  hermes_log(...)    -> src/agent/logger.o
  port_<other>.h     -> src/**/port_<other>.o

Usage:
  python scripts/parity_port_normalize.py <name> [<name> ...]
  python scripts/parity_port_normalize.py --check <name>     # report only

Exit 1 when anything was missing (so it can gate a commit).
"""
import argparse
import glob
import re
import sys
from pathlib import Path

SLERMES = Path("C:/Users/eman5/slermes")

INCLUDE_DEPS = [
    ("hermes_json.h", "lib/libjson/json.o"),
    ("hash.h", "lib/libhash/hash.o"),
    ("hermes_regex.h", "lib/libregex/hermes_regex.o"),
    ("crypto.h", "lib/libcrypto/crypto.o"),
]

DEPS_RE = re.compile(r'^  \[([^\]]+)\]="(.*)"$')


def find_port(name):
    hits = sorted(glob.glob(str(SLERMES / "src" / "**" / f"port_{name}.c"),
                         recursive=True))
    return Path(hits[0]) if hits else None


def derive_deps(port_c, header):
    """The object list the harness link needs, derived from the includes."""
    text = port_c.read_text(encoding="utf-8", errors="replace")
    if header and header.is_file():
        text += "\n" + header.read_text(encoding="utf-8", errors="replace")

    deps = []
    for inc, obj in INCLUDE_DEPS:
        if re.search(rf'^#include\s+"{re.escape(inc)}"', text, re.M):
            deps.append(obj)
    if re.search(r'\bhermes_log\s*\(', text):
        deps.append("src/agent/logger.o")

    # another port's header -> that port's object
    for inc in re.findall(r'^#include\s+"(port_[\w]+\.h)"', text, re.M):
        other = inc[len("port_"):-len(".h")]
        hits = sorted(glob.glob(str(SLERMES / "src" / "**" / f"port_{other}.c"),
                             recursive=True))
        if hits:
            rel = Path(hits[0]).relative_to(SLERMES).with_suffix(".o").as_posix()
            deps.append(rel)

    # stable order, no duplicates, the port's own object first
    own = port_c.relative_to(SLERMES).with_suffix(".o").as_posix()
    ordered = [own] + [d for d in deps if d != own]
    seen, out = set(), []
    for d in ordered:
        if d not in seen:
            seen.add(d)
            out.append(d)
    return out


def ensure_objects_mk(obj_rel):
    mk = SLERMES / "build" / "objects.mk"
    data = mk.read_text(encoding="utf-8", newline="")
    if re.search(rf"(^|\s){re.escape(obj_rel)}(?=\s|\\|$)", data, re.M):
        return False
    # insert beside the other objects of the same directory
    area = obj_rel.rsplit("/", 1)[0]
    m = re.search(rf"(?m)^.*\b{re.escape(area)}/[\w]+\.o .*(?=\\)", data)
    if m:
        line = m.group(0)
        data = data[:m.start()] + line + " " + obj_rel + " \\" + data[m.end():]
    else:
        m = re.search(r"(?m)^( *[\w./]+\.o.*\\)$", data)
        if not m:
            return False
        data = data[:m.end()] + " " + obj_rel + " \\" + data[m.end():]
    mk.write_text(data, encoding="utf-8", newline="")
    return True


def ensure_deps(name, deps):
    sh = SLERMES / "tests" / "run_all_oracles.sh"
    data = sh.read_text(encoding="utf-8", newline="")
    line = f'  [{name}]="{" ".join(deps)}"'
    if re.search(rf'(?m)^  \[{re.escape(name)}\]="', data):
        return "already"
    # insert after the last existing DEPS entry, before the map's closing paren
    entries = list(DEPS_RE.finditer(data))
    if not entries:
        return "no-deps-map"
    last = entries[-1]
    data = data[:last.end()] + "\n" + line + data[last.end():]
    sh.write_text(data, encoding="utf-8", newline="")
    return "added"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("names", nargs="+")
    ap.add_argument("--check", action="store_true",
                    help="report only; do not modify objects.mk / run_all_oracles.sh")
    args = ap.parse_args()

    bad = 0
    for name in args.names:
        port_c = find_port(name)
        if not port_c:
            print(f"{name}: NO src/**/port_{name}.c — nothing to normalize")
            bad += 1
            continue
        header = SLERMES / "include" / f"port_{name}.h"
        deps = derive_deps(port_c, header)
        obj_rel = port_c.relative_to(SLERMES).with_suffix(".o").as_posix()
        print(f"{name}:")
        print(f"  port    {port_c.relative_to(SLERMES)}")
        print(f"  header  {'present' if header.is_file() else 'MISSING'}")
        print(f"  object  {obj_rel}")
        print(f"  deps    {' '.join(deps)}")
        if args.check:
            mk = (SLERMES / "build" / "objects.mk").read_text(
                encoding="utf-8", errors="replace")
            if not re.search(rf"(^|\s){re.escape(obj_rel)}(?=\s|\\|$)", mk, re.M):
                print("  objects.mk  MISSING")
                bad += 1
            else:
                print("  objects.mk  ok")
            sh = (SLERMES / "tests" / "run_all_oracles.sh").read_text(
                encoding="utf-8", errors="replace")
            m = re.search(rf'(?m)^  \[{re.escape(name)}\]="(.*)"$', sh)
            if m:
                have = m.group(1).split()
                missing = [d for d in deps if d not in have]
                if missing:
                    print(f"  DEPS        INCOMPLETE (missing {' '.join(missing)})")
                    bad += 1
                else:
                    print("  DEPS        ok")
            elif port_c.parent == SLERMES / "src" / "cli":
                print("  DEPS        default (src/cli) — ok")
            else:
                print("  DEPS        MISSING")
                bad += 1
            continue
        if ensure_objects_mk(obj_rel):
            print("  objects.mk  added")
        if ensure_deps(name, deps) == "added":
            print("  DEPS        added")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

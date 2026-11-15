#!/usr/bin/env python3
"""Naming gate: enforce the port naming standard.

Standard (docs/dev/naming.md):
  A port unit is named for the PYTHON MODULE it ports, so parity can be audited
  by name rather than by archaeology.

    src/<py_top_dir>/port_<py_module_stem>.c   implementation
    include/port_<py_module_stem>.h            interface
    tests/oracle_<py_module_stem>.py          differential oracle
    tests/t_port_<py_module_stem>.c           C harness for that oracle

  <py_top_dir> is the top-level Python package the module lives in
  (agent/, hermes_cli/, gateway/, pm/, ...).

What this gate enforces -- the invariants parity actually depends on:

  1. A port unit's PoP annotations must name a Python file that EXISTS.
     A port annotated against a module that is not in the tree is claiming
     credit for code nobody can diff against.
  2. A port unit's header, if it has one, must be include/port_<stem>.h.
  3. A port unit in the LINKED tree must not be named *_wrappers.c or
     *_gaps.c -- those are scanner-invisible by design, so parity credit in
     them is unverifiable.

Rename-free: it reports violations instead of rewriting 200+ files, and it
fails the build so the standard cannot silently rot.
"""
from pathlib import Path
import collections
import re
import sys

REPO = Path("C:/Users/eman5/slermes")
QUARRY = Path("C:/Users/eman5/hermes-agent")

# Where the Python sources actually live, in search order. The repo carries its
# own Python tree (agent/, gateway/, hermes_cli/), and the quarry is the upstream
# reference -- a module may legitimately live in either, so both must be checked
# before a PoP target can be called missing.
PY_ROOTS = [REPO, QUARRY, REPO.parent / "hermes-agent"]

POP = re.compile(r"PoP:\s*(\S+)\s*@\s*([A-Za-z0-9_./-]+\.py)\s*:\s*(\S+)")

# The linked tree: objects named in build/objects.mk.
obj_text = (REPO / "build" / "objects.mk").read_text(encoding="utf-8", errors="replace")
LINKED = {Path(m.replace("\\", "/")).with_suffix(".c").as_posix()
          for m in re.findall(r"[\w/]+\.o\b", obj_text)}


def py_exists(mod: str) -> bool:
    rel = mod[:-3].replace(".", "/") + ".py"
    for root in PY_ROOTS:
        if root and (root / rel).exists():
            return True
        # also allow a package-qualified path that omits the top dir
        if root and (root / (mod.split("/")[-1][:-3] + ".py")).exists():
            return True
    return False


def load_legacy_headers() -> set:
    """Port units whose header predates the port_<stem>.h standard.

    Renaming 89 headers means touching every includer, so this is tracked debt
    rather than a mass rename. The gate still fails on NEW violations, so the
    list cannot grow silently. Regenerate with
    scripts/gen_legacy_port_headers.py.
    """
    path = REPO / "tests" / "legacy_port_headers.txt"
    out = set()
    if not path.exists():
        return out
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) == 2:
            out.add((parts[0], parts[1]))
    return out


def load_legacy_pops() -> set:
    """PoP annotations whose module is absent from BOTH trees (verified by a
    full-tree find). See tests/legacy_pop_modules.txt."""
    path = REPO / "tests" / "legacy_pop_modules.txt"
    out = set()
    if not path.exists():
        return out
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) == 2:
            out.add((parts[0], parts[1]))
    return out


def load_header_ownership():
    """Map each unit to the include/ headers it exclusively owns.

    "A unit's own header" cannot be guessed from the filename: 147 of 332 port
    units include no dedicated header at all, several units legitimately own more
    than one, and a filename glob matches unrelated headers (it reported 89
    off-standard where the real number is 49). Ownership -- included by exactly
    one unit -- is the only measurement that means anything.
    """
    inc_re = re.compile(r'#\s*include\s*[<"]([^>"]+)[>"]')
    users = collections.defaultdict(set)
    for c in sorted(REPO.glob("src/**/port_*.c")) + sorted(REPO.glob("src/**/*_ports.c")):
        text = c.read_text(encoding="utf-8", errors="replace")
        if not POP.search(text):
            continue
        rel = c.relative_to(REPO).as_posix()
        for inc in inc_re.findall(text):
            if (REPO / "include" / inc).exists():
                users[inc].add(rel)
    owned = {}
    for inc, rels in users.items():
        if len(rels) == 1:
            owned.setdefault(next(iter(rels)), []).append(Path(inc).name)
    return owned


def main() -> int:
    legacy = load_legacy_headers()
    legacy_pops = load_legacy_pops()
    owned = load_header_ownership()
    units = sorted(REPO.glob("src/**/port_*.c")) + sorted(REPO.glob("src/**/*_ports.c"))
    bad_pop, bad_hdr, bad_name = [], [], []
    legacy_hdr = 0
    checked = 0

    for c in units:
        rel = c.relative_to(REPO).as_posix()
        text = c.read_text(encoding="utf-8", errors="replace")
        pops = POP.findall(text)
        if not pops:
            continue
        checked += 1
        stem = re.sub(r"^port_", "", c.stem)
        stem = re.sub(r"_ports$", "", stem)

        for _fn, mod, _py in pops:
            if not py_exists(mod) and (rel, mod) not in legacy_pops:
                bad_pop.append((rel, mod))

        # A unit's OWN header must follow the standard. Shared utility headers
        # (hermes_json.h, hermes_strutil.h, ...) are included by many units and
        # are not violations; units owning no header are not violations either.
        for hdr_name in owned.get(rel, []):
            if hdr_name != f"port_{stem}.h" and (rel, hdr_name) not in legacy:
                bad_hdr.append((rel, hdr_name))

        if rel in LINKED and (c.name.endswith("_wrappers.c") or c.name.endswith("_gaps.c")):
            bad_name.append(rel)

    print(f"naming gate: {checked} port units carry PoP annotations")
    print(f"  PoP -> missing Python module : {len(bad_pop)}")
    print(f"  header not port_<stem>.h     : {len(bad_hdr)}"
          f"  (+{legacy_hdr} allowlisted legacy)")
    print(f"  scanner-invisible name linked: {len(bad_name)}")

    for rel, mod in bad_pop[:15]:
        print(f"    MISSING PY  {rel}  ->  {mod}")
    for rel, h in bad_hdr[:15]:
        print(f"    ODD HEADER  {rel}  ->  include/{h} (expected port_{rel.split('/')[-1][5:-2]}.h)")
    for rel in bad_name[:15]:
        print(f"    BAD NAME    {rel} is linked but scanner-invisible")

    ok = not (bad_pop or bad_hdr or bad_name)
    print("naming gate:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

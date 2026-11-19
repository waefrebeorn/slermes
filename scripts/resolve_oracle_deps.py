#!/usr/bin/env python3
"""resolve_oracle_deps.py — converge an oracle's DEPS link closure.

Runs tests/run_one_oracle.sh for <name>; while the link reports undefined
references, finds the object that defines each symbol (nm over src/ + lib/)
and appends it to the DEPS entry in tests/run_all_oracles.sh. Repeats until
the harness links (or no new definer exists). SAC-blocked objects are
substituted: lib/libhash/hash.o -> .testbin/hash_O0.o.

Usage: python scripts/resolve_oracle_deps.py <oracle_name> [max_rounds]
Must run inside the winlibs env (source build/winlibs-env.sh) so gcc/nm work.
"""
import os
import re
import subprocess
import sys
import glob

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNNER = os.path.join(REPO, "tests", "run_all_oracles.sh")
# Native Windows "bash" on PATH is the WSL stub (no distro installed) —
# always use the MSYS2 bash that carries the winlibs toolchain.
BASH = "C:/msys64/usr/bin/bash.exe" if os.name == "nt" else "bash"
SAC_SUB = {"lib/libhash/hash.o": ".testbin/hash_O0.o"}
SAC_DROP = {"lib/libregex/hermes_regex.o", "src/agent/auxiliary_client_runtime.o"}
_NM_INDEX = None  # symbol -> defining object, built once per invocation


def deps_line(name):
    txt = open(RUNNER, encoding="utf-8").read()
    m = re.search(r'^\s*\[%s\]="([^"]*)"' % re.escape(name), txt, re.M)
    return (m.group(1).split() if m else []), txt, m


def nm_definers(syms):
    global _NM_INDEX
    if _NM_INDEX is None:
        _NM_INDEX = {}
        objs = sorted(glob.glob(os.path.join(REPO, "src", "**", "*.o"), recursive=True)
                      + glob.glob(os.path.join(REPO, "lib", "**", "*.o"), recursive=True))
        for o in objs:
            rel = os.path.relpath(o, REPO).replace("\\", "/")
            if rel in SAC_DROP:
                continue
            try:
                out = subprocess.run(["nm", "--defined-only", o],
                                     capture_output=True, text=True, timeout=60).stdout
            except Exception:
                continue
            for l in out.splitlines():
                if " T " in l:
                    _NM_INDEX.setdefault(l.split()[-1], SAC_SUB.get(rel, rel))
    return {s: _NM_INDEX[s] for s in syms if s in _NM_INDEX}


def main():
    name = sys.argv[1]
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    os.chdir(REPO)
    for i in range(rounds):
        deps, txt, m = deps_line(name)
        if m is None:
            print("no DEPS entry for", name)
            return 2
        r = subprocess.run(
            [BASH, "tests/run_one_oracle.sh", name,
             "sta_oracle_%s.py" % name, "", " ".join(deps)],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
            timeout=600)
        out = (r.stdout or "") + (r.stderr or "")
        cm = re.search(r"(\d+) cases, (\d+) mismatches", out)
        if r.returncode == 0 and cm and int(cm.group(1)) > 0 and int(cm.group(2)) == 0:
            print("ROUND %d: GREEN (%s cases)\n%s" % (i, cm.group(1), out.strip()[-400:]))
            return 0
        if "PYERR" in out:
            print("ROUND %d: PYERR — oracle-side failure, not a link issue:\n%s"
                  % (i, out.strip()[-600:]))
            return 1
        missing = sorted(set(re.findall(
            r"undefined reference to [`\"']([A-Za-z_][A-Za-z0-9_]*)", out)))
        if not missing:
            print("ROUND %d: no undefined refs; rc=%d\n%s"
                  % (i, r.returncode, out.strip()[-800:]))
            return 1
        found = nm_definers(missing)
        new = [o for o in sorted(set(found.values())) if o not in deps]
        unresolved = [s for s in missing if s not in found]
        print("ROUND %d: %d undefined, +%d objects, unresolved: %s"
              % (i, len(missing), len(new), unresolved[:8]))
        if not new:
            print(out.strip()[-800:])
            return 1
        deps += new
        # infrastructure_stubs.o must stay LAST (real definitions win).
        if "src/infrastructure_stubs.o" in deps:
            deps.remove("src/infrastructure_stubs.o")
            deps.append("src/infrastructure_stubs.o")
        new_line = '  [%s]="%s"' % (name, " ".join(deps))
        txt = txt[:m.start()] + new_line + txt[m.end():]
        open(RUNNER, "w", encoding="utf-8", newline="\n").write(txt)
    print("did not converge in %d rounds" % rounds)
    return 1


if __name__ == "__main__":
    sys.exit(main())

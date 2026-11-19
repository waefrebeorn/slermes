#!/usr/bin/env python3
"""reorg_c_mirror.py — move C ports so the src/ tree mirrors the quarry's
CURRENT package layout (user directive: change old to fit new schema).

Driven by PoP annotations: a C file whose claims all live in one quarry
directory moves to src/<that directory>. Plus the explicit
persistence/sqlite_state -> persistence/sqlite/_state mirror.

Rewrites every path reference: build/*.mk, DEPS, gate.json extra_src/cflags,
pyport_map, harness/oracle path strings, C #include path forms. Deletes the
stale .o at each old path (the FULL-link oracle tier globs src/**/*.o and
would otherwise double-define). Bumps SRCINCS depth for the deeper mirror.

Usage: python scripts/reorg_c_mirror.py [--apply]
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
APPLY = "--apply" in sys.argv
# WindowsApps blocks the bundled git; use the portable copy.
_pg = Path.home() / "AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/bin/git.exe"
GIT = str(_pg) if _pg.is_file() else "git"
if _pg.is_file():
    os.environ["GIT_EXEC_PATH"] = str(
        Path.home() / "AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/libexec/git-core")
MOVES_FILE = Path(os.environ.get("TEMP", "/tmp")) / "c_mirror_moves.json"
SCRATCH = Path.home() / "AppData/Local/hermes/cache/scratch/c_mirror_moves.json"

# Files the parallel porting agents are editing — never move these.
AGENT_FILES = {
    "src/agent/port_model_metadata.c", "src/agent/model_metadata.c",
    "src/agent/display_ports.c", "src/agent/display_helpers.c",
    "src/hermes_cli/config_py_pure.c", "src/hermes_cli/port_config.c",
    "src/hermes_cli/port_config_nested.c",
    "src/tools/process_registry_gaps.c", "src/tools/process_registry.c",
}


def main():
    moves = json.loads(SCRATCH.read_text(encoding="utf-8"))
    # Explicit mirror: the whole sqlite_state cluster -> sqlite/_state
    for p in (REPO / "src/persistence/sqlite_state").glob("*.c"):
        rel = p.relative_to(REPO).as_posix()
        moves[rel] = "src/persistence/sqlite/_state/" + p.name
    moves = {k: v for k, v in moves.items()
             if k not in AGENT_FILES and (REPO / k).is_file()}
    print("moves:", len(moves))

    # replacement pairs: .c and .o, slash and backslash forms
    pairs = {}
    for old, new in moves.items():
        for suf in (".c", ".o"):
            o = old[:-2] + suf if old.endswith(".c") else old
            n = new[:-2] + suf if new.endswith(".c") else new
            pairs[o] = n
            pairs[o.replace("/", "\\")] = n.replace("/", "\\")
    pat = re.compile("|".join(re.escape(k) for k in
                              sorted(pairs, key=len, reverse=True)))

    def rewrite_file(p):
        try:
            t = p.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            return 0
        t2, n = pat.subn(lambda m: pairs[m.group(0)], t)
        if n and APPLY:
            p.write_text(t2, encoding="utf-8", newline="\n")
        return n

    targets = []
    targets += [REPO / "Makefile"]
    targets += list((REPO / "build").glob("*.mk"))
    targets += [REPO / "tests/run_all_oracles.sh", REPO / "tests/run_one_oracle.sh",
                REPO / "scripts/pyport_map.json"]
    targets += list((REPO / "tests").glob("*.c"))
    targets += list((REPO / "tests").glob("*.py"))
    targets += list((REPO / "tests/oracle").glob("*.py"))
    targets += list((REPO / "src").rglob("*.c"))
    targets += list((REPO / "src").rglob("*.h"))
    targets += list((REPO / "include").rglob("*.h"))
    targets += list((REPO / "scripts").glob("*.py"))
    gates = list((REPO / "tests/watercup/gates").glob("*/gate.json"))
    targets += gates

    total = 0
    changed = 0
    agent_abs = {str(REPO / f) for f in AGENT_FILES}
    for p in targets:
        if str(p) in agent_abs:
            continue  # parallel porting agents own these right now
        n = rewrite_file(p)
        if n:
            total += n
            changed += 1
    print("path refs rewritten:", total, "in", changed, "files")

    if APPLY:
        collisions = []
        for old, new in moves.items():
            src = REPO / old
            dst = REPO / new
            if not src.exists():
                continue  # already moved (idempotent re-run)
            if dst.exists():
                collisions.append((old, new))
                continue  # namesake already lives there — manual merge later
            dst.parent.mkdir(parents=True, exist_ok=True)
            r = subprocess.run([GIT, "mv", old, new], cwd=REPO,
                               capture_output=True, text=True)
            if r.returncode != 0:
                # fall back to plain move (untracked file)
                src.rename(dst)
            stale = src.with_suffix(".o")
            if stale.exists():
                stale.unlink()
        # SRCINCS depth: the mirror nests deeper than 3 (plugins/platforms/x,
        # hermes_cli/proxy/adapters, persistence/sqlite/_state).
        for f in ("tests/run_one_oracle.sh",):
            p = REPO / f
            t = p.read_text(encoding="utf-8")
            t = t.replace("find src -maxdepth 3 -type d", "find src -maxdepth 6 -type d")
            p.write_text(t, encoding="utf-8", newline="\n")
        cache = REPO / ".testbin/srcincs.cache"
        if cache.exists():
            cache.unlink()
        print("moved", len(moves) - len(collisions), "files; stale objects removed; SRCINCS depth 6")
        if collisions:
            # References were rewritten to the new path, but the file stayed
            # put (namesake collision) — revert those refs so the build holds.
            rev = {}
            for old, new in collisions:
                for suf in (".c", ".o"):
                    o = old[:-2] + suf if old.endswith(".c") else old
                    n = new[:-2] + suf if new.endswith(".c") else new
                    rev[n] = o
                    rev[n.replace("/", "\\")] = o.replace("/", "\\")
            rpat = re.compile("|".join(re.escape(k) for k in
                                       sorted(rev, key=len, reverse=True)))
            for p in targets:
                if str(p) in agent_abs:
                    continue
                try:
                    t = p.read_text(encoding="utf-8")
                except (UnicodeDecodeError, OSError):
                    continue
                t2, n = rpat.subn(lambda m: rev[m.group(0)], t)
                if n:
                    p.write_text(t2, encoding="utf-8", newline="\n")
            print("COLLISIONS (kept old path, refs reverted):")
            for old, new in collisions:
                print("  ", old, "!!", new)
    else:
        print("DRY RUN (pass --apply)")


if __name__ == "__main__":
    main()

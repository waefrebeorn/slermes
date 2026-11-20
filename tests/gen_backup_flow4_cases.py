#!/usr/bin/env python3
"""Generate tests/fixtures/backup_flow4.jsonl -- scripted-seam FLOW cases for
the last UNCLAIMED backup.py def: the two nested `_lock_op` closures inside
`_backup_operation_lock`, ported as src/cli/backup_flow4.c.

Every case: {"func": "lock_op", "in": {"os":..., "flag":..., "seams": {...}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). {"__raise__": cls, "msg": m} raises that builtin in the live drive and
is delivered as a raise descriptor to the C universal seam. A MISSING seam key
fail-closes (RuntimeError) on BOTH sides; a present JSON null is a valid None.
{"__arr__": v} is an explicit array VALUE, not a queue.

Cases cover both platform branches (nt / posix), success paths, fail-closed
unscripted seams, fd=0 / fd=null edge values, and the full builtin exception
hierarchy (OSError subclasses PermissionError/FileNotFoundError, plus the
BaseException trio KeyboardInterrupt / SystemExit / GeneratorExit that must
propagate through the `except BaseException` recorder unchanged).

Run: python tests/gen_backup_flow4_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "backup_flow4.jsonl")

CASES = []


def add(osname, flag, seams):
    inp = {"os": osname, "flag": flag, "seams": seams}
    CASES.append({"func": "lock_op", "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


# ── nt branch: seek(0) -> fileno() -> msvcrt.locking(fd, flag, 1) ──
# 1. plain success
add("nt", 1, {"handle.seek": None, "handle.fileno": 5, "msvcrt.locking": None})
# 2. unlock flag, different fd
add("nt", 8, {"handle.seek": None, "handle.fileno": 3, "msvcrt.locking": None})
# 3. seek raises OSError -> propagate before fileno/locking
add("nt", 1, {"handle.seek": rs("OSError", "seek fail")})
# 4. fileno raises PermissionError (OSError subclass) -> propagate
add("nt", 1, {"handle.seek": None, "handle.fileno": rs("PermissionError", "no fd")})
# 5. locking raises OSError -> propagate after all three seams fired
add("nt", 1, {"handle.seek": None, "handle.fileno": 5,
              "msvcrt.locking": rs("OSError", "lock fail")})
# 6. locking raises KeyboardInterrupt (BaseException trio) -> propagate
add("nt", 1, {"handle.seek": None, "handle.fileno": 5,
              "msvcrt.locking": rs("KeyboardInterrupt", "stop")})
# 7. fd = 0 (falsy but valid) flows into locking
add("nt", 2, {"handle.seek": None, "handle.fileno": 0, "msvcrt.locking": None})
# 8. fd = null -> dropped from recorded args on both sides (present None is valid)
add("nt", 1, {"handle.seek": None, "handle.fileno": None, "msvcrt.locking": None})
# 9. fail-closed: missing seam -> RuntimeError "unscripted seam: handle.seek"
add("nt", 1, {})

# ── posix branch: fileno() -> fcntl.flock(fd, flag) ──
# 10. plain success (LOCK_EX | LOCK_NB = 6)
add("posix", 6, {"handle.fileno": 5, "fcntl.flock": None})
# 11. unlock flag (LOCK_UN = 8)
add("posix", 8, {"handle.fileno": 3, "fcntl.flock": None})
# 12. fileno raises FileNotFoundError (OSError subclass) -> propagate
add("posix", 6, {"handle.fileno": rs("FileNotFoundError", "gone")})
# 13. flock raises OSError -> propagate after both seams fired
add("posix", 6, {"handle.fileno": 5, "fcntl.flock": rs("OSError", "flock fail")})
# 14. flock raises SystemExit (BaseException trio) -> propagate
add("posix", 6, {"handle.fileno": 5, "fcntl.flock": rs("SystemExit", "bye")})
# 15. flag = 0 edge
add("posix", 0, {"handle.fileno": 5, "fcntl.flock": None})
# 16. flock raises PermissionError -> propagate
add("posix", 6, {"handle.fileno": 5, "fcntl.flock": rs("PermissionError", "denied")})
# 17. flock raises GeneratorExit (BaseException trio) -> propagate
add("posix", 6, {"handle.fileno": 7, "fcntl.flock": rs("GeneratorExit", "gen")})
# 18. fail-closed: missing seam -> RuntimeError "unscripted seam: handle.fileno"
add("posix", 6, {})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    thin = {k: v for k, v in counts.items() if v < 5}
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()

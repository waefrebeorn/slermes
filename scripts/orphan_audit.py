"""Find ported .c files that are not in the product.

`build/objects.mk` is the link manifest: a translation unit absent from it contributes
nothing to `slermes.exe`, no matter how complete it looks. `src/mcp_serve.c` -- a 400+ line
HTTP server -- was never listed, while four one-line stubs in `infrastructure_stubs.c`
shadowed its API, so `mcp-serve` could only ever print "Failed to start server". One
orphan found by accident; this tool finds them all.

Usage:
  python scripts/orphan_audit.py                # list orphans, grouped by tree
  python scripts/orphan_audit.py --compile      # and try to compile each one
  python scripts/orphan_audit.py --json         # machine-readable
"""
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OBJ_MK = REPO / "build" / "objects.mk"
SRC = REPO / "src"


def linked_objects() -> set:
    text = OBJ_MK.read_text(encoding="utf-8", errors="replace")
    return set(re.findall(r"(src/[\w./-]+\.o)\b", text))


def orphans() -> list:
    objs = linked_objects()
    out = []
    for path in sorted(SRC.rglob("*.c")):
        rel = path.relative_to(REPO).as_posix()
        if rel[:-2] + ".o" in objs:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        defs = len(re.findall(r"^\}\s*$", text, re.MULTILINE))
        has_pop = bool(re.search(r"^\s*\*?\s*PoP:", text, re.MULTILINE))
        out.append({"file": rel, "bytes": len(text), "defs": defs, "pop": has_pop})
    return out


def try_compile(files) -> dict:
    """Compile in one make invocation; report which objects appeared."""
    res = {}
    args = ["make", "-j4", "-k"] + [f[:-2] + ".o" for f in files]
    proc = subprocess.run(args, cwd=str(REPO), capture_output=True, text=True,
                          errors="replace")
    log = (proc.stdout or "") + (proc.stderr or "")
    for f in files:
        res[f] = (REPO / (f[:-2] + ".o")).exists()
    res["_errors"] = [ln for ln in log.splitlines() if " error:" in ln][:20]
    return res


def main(argv):
    rows = orphans()
    by_tree = {}
    for r in rows:
        tree = "/".join(r["file"].split("/")[:2])
        by_tree.setdefault(tree, []).append(r)
    if "--json" in argv:
        print(json.dumps({"count": len(rows), "rows": rows}, indent=2))
        return 0

    total_kb = sum(r["bytes"] for r in rows) / 1024
    print("orphans: %d files, %.0f KB not in build/objects.mk\n" % (len(rows), total_kb))
    print("%-34s %5s %8s" % ("tree", "files", "KB"))
    for tree, rs in sorted(by_tree.items(), key=lambda kv: -sum(r["bytes"] for r in kv[1])):
        print("%-34s %5d %8.0f" % (tree, len(rs), sum(r["bytes"] for r in rs) / 1024))
    print("\nlargest (a big orphan is a whole feature that never shipped):")
    for r in sorted(rows, key=lambda r: -r["bytes"])[:12]:
        print("  %-58s %6.1f KB %3d defs %s"
              % (r["file"], r["bytes"] / 1024, r["defs"], "PoP" if r["pop"] else "---"))

    if "--compile" in argv:
        print("\ncompiling each orphan (-k, parallel)...")
        res = try_compile([r["file"] for r in rows])
        good = [f for f, ok in res.items() if ok is True]
        print("  compiles clean : %d" % len(good))
        print("  fails to build : %d" % (len(rows) - len(good)))
        for e in res["_errors"][:8]:
            print("   ", e[:140])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

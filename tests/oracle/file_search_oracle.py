#!/usr/bin/env python3
"""Differential oracle for `search_files` (src/tools/file_search.c).

Why this exists: the tool used to shell out to `grep -rn ... 2>/dev/null | head -50`. On a machine
without grep on PATH that answers "no matches" -- the single most misleading answer a search tool can
give, because the caller concludes the code does not exist. It also ignored its own documented
`target`/`output_mode`/`limit` arguments and reported match_count as 0 or 1.

So this compares the C implementation against Python doing the same search over the same fixture tree,
case by case, and refuses to accept "0 matches" as agreement unless Python also finds 0.

Usage: python tests/oracle/file_search_oracle.py [--keep]
"""
from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / ".testbin" / "file_search_diff.exe"

SKIP_DIRS = {".git", "node_modules", "__pycache__", ".venv", "venv", ".mypy_cache",
             ".pytest_cache", ".testbin", "build", ".cache", "dist", ".idea"}

FIXTURE = {
    "alpha.c": "int alpha(void) {\n    return 42;  /* TODO: replace */\n}\n",
    "beta.c": "int beta(void) {\n    /* TODO: also */\n    return alpha();\n}\n",
    "notes.md": "alpha appears here too\nTODO in markdown\n",
    "sub/gamma.h": "#define GAMMA_ALPHA 1\n",
    "sub/deep/delta.c": "// TODO: nested\nalpha();\n",
    "bin/raw.bin": "\x00alpha\x00",
}


def build() -> Path:
    if EXE.exists():
        return EXE
    # The build's own compiler: the msys2 one declares strndup in <string.h>, which collides with the
    # windows shim in include/hermes_win32.h. Same toolchain as the product or the harness is a lie.
    cc = "C:/winlibs/mingw64/bin/gcc.exe"
    if not Path(cc).exists():
        cc = "gcc"
    cmd = [cc, "-O1", "-w", "-include", "hermes_win32.h", "-D_GNU_SOURCE",
           "-I", "include/compat", "-I", "include", "-I", "src", "-I", "src/tools",
           "-I", "lib", "-I", "lib/libjson", "-I", "lib/libregex",
           "src/tools/file_search.c", "tests/unit/file_search_diff.c",
           "lib/libjson/json.o", "lib/libregex/hermes_regex.o",
           "-L", "C:/winlibs/extra-libs", "-ltre", "-o", str(EXE)]
    r = subprocess.run(cmd, cwd=str(REPO), capture_output=True, text=True)
    if r.returncode != 0:
        print("cannot build the harness:\n" + (r.stderr or "")[:800])
        sys.exit(2)
    return EXE


def py_search(root: Path, pattern: str, glob: str | None, target: str, mode: str, limit: int):
    """Python's version of the same contract -- deliberately simple, so agreement means something."""
    rx = re.compile(pattern) if target == "content" else None
    matches, files, count, truncated = [], 0, 0, False

    def walk(d: Path, rel: str):
        nonlocal files, count, truncated
        for entry in sorted(d.iterdir()):
            if entry.name in SKIP_DIRS:
                continue
            label = f"{rel}/{entry.name}" if rel else entry.name
            if entry.is_dir():
                walk(entry, label)
                continue
            if glob and not Path(entry.name if "/" not in glob else label).match(glob):
                continue
            if target == "files":
                if count >= limit:
                    truncated = True
                    return
                matches.append(label)
                count += 1
                files += 1
                continue
            try:
                data = entry.read_bytes()
            except OSError:
                continue
            if b"\x00" in data[:512]:
                continue
            hits = 0
            for lineno, line in enumerate(data.decode("utf-8", "ignore").splitlines(), 1):
                if not rx.search(line):
                    continue
                if mode == "content" and count >= limit:
                    # Same rule as the C side: the limit is on matches REPORTED.
                    truncated = True
                    break
                hits += 1
                count += 1
                if mode == "content":
                    matches.append(f"{label}:{lineno}:{line}")
            if hits and mode == "files_only":
                matches.append(label)
            if hits:
                files += 1

    walk(root, "")
    return {"match_count": count, "file_count": files, "truncated": truncated, "matches": matches}


def main() -> int:
    keep = "--keep" in sys.argv
    exe = build()
    root = Path(tempfile.mkdtemp(prefix="fs_oracle_"))
    try:
        for rel, content in FIXTURE.items():
            p = root / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(content.encode("utf-8") if "\x00" not in content else content.encode("latin-1"))

        agree = failed = 0
        cases = [
            ("content", "alpha", None, "content", "content", 50),
            ("glob-filter", "alpha", "*.c", "content", "content", 50),
            ("path-glob", "TODO", "sub/*.c", "content", "content", 50),
            ("files-only", "TODO", None, "content", "files_only", 50),
            ("count", "TODO", None, "content", "count", 50),
            ("files-mode", "", "*.c", "files", "content", 50),
            ("limit", "alpha", None, "content", "content", 2),
            ("regex-class", "[A-Z]+_ALPHA", None, "content", "content", 50),
            ("no-hits", "zzz_absent_zzz", None, "content", "content", 50),
            ("binary-skip", "alpha", None, "content", "content", 50),
        ]

        for name, pattern, glob, target, mode, limit in cases:
            args = {"pattern": pattern or ".", "path": str(root), "target": target,
                    "output_mode": mode, "limit": limit}
            if glob:
                args["file_glob"] = glob
            argf = root.parent / f"fs_oracle_{root.name}_args.json"   # outside the tree being searched
            argf.write_text(json.dumps(args), encoding="utf-8")
            raw = subprocess.run([str(exe), str(argf)], capture_output=True, text=True).stdout
            try:
                have = json.loads(raw)
            except json.JSONDecodeError:
                print(f"FAIL {name}: harness printed non-JSON: {raw[:120]}")
                failed += 1
                continue

            want = py_search(root, pattern or ".", glob, target, mode, limit)
            got = {"match_count": have.get("match_count"), "file_count": have.get("file_count"),
                   "truncated": have.get("truncated"),
                   "matches": (have.get("matches") or "").splitlines() if have.get("matches") else []}
            if have.get("target") == "files" or (target == "files"):
                got["matches"] = (have.get("matches") or "").splitlines() if have.get("matches") else []

            problems = []
            if got["match_count"] != want["match_count"]:
                problems.append(f"match_count {got['match_count']} != {want['match_count']}")
            if got["file_count"] != want["file_count"]:
                problems.append(f"file_count {got['file_count']} != {want['file_count']}")
            if bool(got["truncated"]) != want["truncated"]:
                problems.append(f"truncated {got['truncated']} != {want['truncated']}")
            if sorted(got["matches"]) != sorted(want["matches"]):
                problems.append(f"matches differ: {len(got['matches'])} vs {len(want['matches'])}")
                for a, b in list(zip(got["matches"], want["matches"]))[:2]:
                    if a != b:
                        problems.append(f"   C : {a[:100]}")
                        problems.append(f"   py: {b[:100]}")
                        break

            if problems:
                failed += 1
                print(f"FAIL {name}: " + "; ".join(problems))
            else:
                agree += 1
                # A search that finds nothing must be shown to be right, not assumed right.
                if want["match_count"] == 0:
                    print(f"  note: {name} legitimately has 0 matches (both sides)")

        print(f"file-search oracle: {agree} agree, {failed} problem(s)")
        return 0 if failed == 0 else 1
    finally:
        if keep:
            print(f"fixture kept at {root}")
        else:
            shutil.rmtree(root, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())

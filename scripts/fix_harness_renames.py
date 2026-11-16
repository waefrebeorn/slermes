#!/usr/bin/env python3
"""Repoint harnesses at the post-rename function names.

The reserved-underscore rename (commit 9d3197f83a) renamed 1,634 file-scope
`_foo` definitions to `foo`, but the auto-generated oracle harnesses still
called the old names. gcc says so explicitly:

    implicit declaration of function '_discover_skills_in_dir';
    did you mean 'discover_skills_in_dir'?

This reads the compiler's own suggestion out of each failing harness's build
log and applies it, so the mapping comes from the compiler rather than guesses.

Usage: fix_harness_renames.py <logfile>...
"""
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
SUGGEST = re.compile(
    r"implicit declaration of function '(_?\w+)'; did you mean '(\w+)'?")


def main(logs):
    # Defined symbols in the first-party tree, so a suggestion is only applied
    # if the target really exists. gcc's "did you mean" is fuzzy and has
    # suggested unrelated functions (process_registry_get_session for
    # process_registry_count_running).
    import re as _re
    defined = set()
    for base in ("src", "lib", "include"):
        for f in (REPO / base).rglob("*"):
            if f.suffix not in (".c", ".h"):
                continue
            if "whisper_cpp" in f.as_posix() or "ctranslate2_src" in f.as_posix():
                continue
            try:
                txt = f.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for m in _re.finditer(r"^[A-Za-z_][\w\s\*]*\b([A-Za-z_]\w*)\s*\(", txt,
                                  _re.M):
                defined.add(m.group(1))

    total = 0
    for log in logs:
        text = Path(log).read_text(encoding="utf-8", errors="replace")
        pairs = set(SUGGEST.findall(text))
        if not pairs:
            continue
        pairs = {(o, n) for o, n in pairs if n in defined and o not in defined}
        if not pairs:
            print("  no verified renames in", log)
            continue
        m = re.search(r"(tests/t_port_\w+\.c)", text)
        if not m:
            print("  no harness path in", log)
            continue
        f = REPO / m.group(1)
        if not f.exists():
            print("  missing", f)
            continue
        t = f.read_text(encoding="utf-8")
        n = 0
        for old, new in pairs:
            if old == new:
                continue
            t2 = re.sub(r"\b" + re.escape(old) + r"\b", new, t)
            if t2 != t:
                n += 1
                t = t2
        if n:
            f.write_text(t, encoding="utf-8")
            total += n
            print(f"  {f.name}: {n} rename(s) {sorted(pairs)}")
    print("harness renames applied:", total)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

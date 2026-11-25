"""Wire orphans by link closure: add candidates, link, drop whoever broke it, repeat.

`orphan_audit.py` finds candidates (not in objects.mk) and `wire_orphans.py` drops the ones
whose symbols already ship. What remains (732 files) does NOT link as a block: a first
attempt gave 26 `multiple definition` and 375 `undefined reference` errors, because the
candidates reference each other and some pull in libraries the target does not use
(ncurses, a `tui` global).

So converge instead of guessing: add every candidate, link, read the linker's own verdict,
drop the objects it named, and repeat. Each pass keeps strictly more valid code than the
last and the loop terminates when the link is clean.

Usage:
  python scripts/wire_closure.py --max-passes 8 [--tree src/agent] [--write]
Without --write the winning set is reported but objects.mk is left alone.
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"
STUB_ANCHOR = "src/infrastructure_stubs.o"
OBJ_LINE = re.compile(r"^(\S+\.o):\S+\(\.text[^)]*\):\s*(?:undefined reference to|multiple definition of)", re.M)
BAD_NAMED = re.compile(r"^([^\s:]+\.o)\b")


def link() -> tuple:
    proc = subprocess.run(
        ["make", "-j8", "slermes", 'USERPROFILE=C:\\Users\\eman5',
         'LOCALAPPDATA=C:\\Users\\eman5\\AppData\\Local'],
        cwd=str(REPO), capture_output=True, text=True, errors="replace")
    log = (proc.stdout or "") + (proc.stderr or "")
    try:
        (REPO / ".testbin" / "closure_last.log").write_text(log, encoding="utf-8")
    except OSError:
        pass
    return proc.returncode, log


def offenders(log: str) -> set:
    """Objects the linker complained about, as repo-relative paths."""
    out = set()
    for line in log.splitlines():
        if "undefined reference" not in line and "multiple definition" not in line:
            continue
        line = line.strip()
        # The line begins with the absolute path to ld.exe, which itself ends in ".exe:".
        # Matching the first "*.o:" on the raw line captured "ld.exe" and the loop could
        # never identify a guilty object -- that is why it reported "stuck".
        if ".exe: " in line:
            line = line.split(".exe: ", 1)[1]
        m = re.match(r"([\w./-]+\.o):", line)
        if m:
            out.add(m.group(1).lstrip("./"))
    return out


def main(argv):
    write = "--write" in argv
    max_passes = 8
    if "--max-passes" in argv:
        max_passes = int(argv[argv.index("--max-passes") + 1])
    tree = argv[argv.index("--tree") + 1] if "--tree" in argv else None

    text = MK.read_text(encoding="utf-8", errors="replace")
    linked = set(re.findall(r"(src/[\w./-]+\.o)", text))
    cand = []
    for p in sorted((REPO / "src").rglob("*.c")):
        rel = p.relative_to(REPO).as_posix()
        if rel[:-2] + ".o" in linked:
            continue
        if tree and not rel.startswith(tree):
            continue
        if (REPO / (rel[:-2] + ".o")).exists():
            cand.append(rel[:-2] + ".o")

    print("candidates: %d" % len(cand))
    keep = set(cand)
    for p in range(1, max_passes + 1):
        body = " ".join(sorted(keep))
        trial = text.replace(STUB_ANCHOR, body + " " + STUB_ANCHOR, 1) if keep else text
        MK.write_text(trial, encoding="utf-8", newline="")
        rc, log = link()
        bad = offenders(log) & keep
        print("pass %d: rc=%d bad=%d keep=%d" % (p, rc, len(bad), len(keep)))
        if rc == 0:
            print("CLEAN with %d objects wired" % len(keep))
            if not write:
                MK.write_text(text, encoding="utf-8", newline="")
                print("(--write not given: objects.mk restored)")
            return 0
        if not bad:
            print("stuck: link fails but names no candidate object")
            print(log[-1500:])
            MK.write_text(text, encoding="utf-8", newline="")
            return 1
        keep -= bad

    MK.write_text(text, encoding="utf-8", newline="")
    print("gave up after %d passes; objects.mk restored" % max_passes)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))

"""Prune objects from objects.mk that collide with code already in the product.

After wiring 877 orphans the link was clean, but a rebuild produced 566 `multiple definition`
errors: src/*.o translations that duplicate symbols already provided by lib/*.o (threat
patterns, tui) or by a monolith. They were wired on an earlier link pass whose object files
came from the orphan audit build; once make rebuilt from source the collision surfaced.

The rule this enforces: the LIB and the monoliths are the product; a src file that redefines
their symbols is a superseded split, not new behaviour. Drop the src object, never the lib.

Usage: python scripts/prune_dupes.py [--max-passes 5]
"""
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"


def link():
    p = subprocess.run(["make", "-j8", "slermes", 'USERPROFILE=C:\\Users\\eman5',
                        'LOCALAPPDATA=C:\\Users\\eman5\\AppData\\Local'],
                       cwd=str(REPO), capture_output=True, text=True, errors="replace")
    log = ((p.stdout or "") + (p.stderr or ""))
    (REPO / ".testbin").mkdir(exist_ok=True)
    (REPO / ".testbin" / "prune_last.log").write_text(log, encoding="utf-8")
    return p.returncode, log


def duplicates(log):
    """-> set of src objects that define a symbol the product already has."""
    out = set()
    for line in log.splitlines():
        if "multiple definition" not in line or ".exe: " not in line:
            continue
        body = line.split(".exe: ", 1)[1]
        m = re.match(r"([\w./-]+\.o):", body)
        if not m:
            continue
        obj = m.group(1).lstrip("./")
        # the FIRST object named is the one ld saw second; either way, a duplicate between a
        # src object and a lib object must drop the src one.
        pair = re.findall(r"([\w./-]+\.o):", body)
        drop = [o.lstrip("./") for o in pair if o.startswith("src/")]
        out.update(drop)
    return out


def main(argv):
    maxp = int(argv[argv.index("--max-passes") + 1]) if "--max-passes" in argv else 5
    for p in range(1, maxp + 1):
        text = MK.read_text(encoding="utf-8", errors="replace")
        rc, log = link()
        if rc == 0:
            print("pass %d: CLEAN" % p)
            return 0
        drop = duplicates(log)
        wired = set(re.findall(r"(src/[\w./-]+\.o)", text))
        drop &= wired
        print("pass %d: rc=%d drop=%d wired=%d" % (p, rc, len(drop), len(wired)))
        if not drop:
            print("  no src object to blame; last errors:")
            for ln in log.splitlines():
                if "error" in ln.lower():
                    print("   ", ln[:160])
                    break
            return 1
        for o in drop:
            text = re.sub(r"\s*" + re.escape(o) + r"\b", "", text)
        MK.write_text(text, encoding="utf-8", newline="")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))

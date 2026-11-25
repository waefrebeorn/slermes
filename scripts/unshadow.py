"""Wire orphans while retiring the stubs they replace -- as one atomic unit.

`wire_closure.py` wires candidates and drops whichever object the linker names. That loop
cannot finish: `src/infrastructure_stubs.o` defines 56 symbols that the candidate set also
defines, and the stub file is not a candidate, so no object is ever named as guilty.

Retiring the stubs on their own is worse: measured, removing the 41 single-line stubs whose
real implementations exist gave LINK_RC=2 with 41 undefined references, because those stubs
were serving already-linked callers. A stub is a placeholder holding a live socket.

So decide by PAIR, per pass:
  * `multiple definition of \`X'` blamed on infrastructure_stubs.o -> comment out the stub
    for X, rebuild that object, retry with the same candidate set (nothing is lost: the real
    implementation is in the set, which is why the collision happened).
  * `undefined reference` / `multiple definition` blamed on a candidate -> drop the candidate.
  * link clean -> keep the set and write objects.mk.

Both files are backed up and restored if the loop cannot reach a clean link.

Usage: python scripts/unshadow.py [--max-passes 10] [--write]
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MK = REPO / "build" / "objects.mk"
STUB_C = REPO / "src" / "infrastructure_stubs.c"
STUB_O = REPO / "src" / "infrastructure_stubs.o"
ANCHOR = "src/infrastructure_stubs.o"


def link():
    proc = subprocess.run(
        ["make", "-j8", "slermes", 'USERPROFILE=C:\\Users\\eman5',
         'LOCALAPPDATA=C:\\Users\\eman5\\AppData\\Local'],
        cwd=str(REPO), capture_output=True, text=True, errors="replace")
    log = (proc.stdout or "") + (proc.stderr or "")
    (REPO / ".testbin").mkdir(exist_ok=True)
    (REPO / ".testbin" / "unshadow_last.log").write_text(log, encoding="utf-8")
    return proc.returncode, log


def parse(log):
    """-> (stub_symbols, candidate_objects)"""
    stub_syms, cand_objs = set(), set()
    for line in log.splitlines():
        if ".exe: " in line:
            line = line.split(".exe: ", 1)[1]
        m = re.match(r"([\w./-]+\.o):.*multiple definition of `([^']+)'", line)
        if m:
            if m.group(1).endswith("infrastructure_stubs.o"):
                stub_syms.add(m.group(2))
            else:
                cand_objs.add(m.group(1).lstrip("./"))
            continue
        m = re.match(r"([\w./-]+\.o):.*undefined reference to `([^']+)'", line)
        if m and not m.group(1).endswith("infrastructure_stubs.o"):
            cand_objs.add(m.group(1).lstrip("./"))
    return stub_syms, cand_objs


def retire(syms):
    """Comment out one-line stub definitions for these symbols. Returns count."""
    text = STUB_C.read_text(encoding="utf-8", errors="replace")
    out, n = [], 0
    for line in text.splitlines(keepends=True):
        m = re.match(r"^[\w \*]*\b([A-Za-z_]\w*)\s*\([^;]*\)\s*\{.*\}\s*$", line)
        if m and m.group(1) in syms:
            out.append("/* RETIRED stub, real implementation wired: %s\n */\n" % line.rstrip())
            n += 1
        else:
            out.append(line)
    if n:
        STUB_C.write_text("".join(out), encoding="utf-8", newline="")
        if STUB_O.exists():
            STUB_O.unlink()
    return n


def main(argv):
    write = "--write" in argv
    maxp = int(argv[argv.index("--max-passes") + 1]) if "--max-passes" in argv else 10

    mk_bak = MK.read_text(encoding="utf-8", errors="replace")
    stub_bak = STUB_C.read_text(encoding="utf-8", errors="replace")

    linked = set(re.findall(r"(src/[\w./-]+\.o)", mk_bak))
    keep = set()
    for p in sorted((REPO / "src").rglob("*.c")):
        rel = p.relative_to(REPO).as_posix()
        if rel[:-2] + ".o" in linked:
            continue
        if (REPO / (rel[:-2] + ".o")).exists():
            keep.add(rel[:-2] + ".o")
    print("candidates: %d" % len(keep))

    for p in range(1, maxp + 1):
        body = " ".join(sorted(keep))
        MK.write_text(mk_bak.replace(ANCHOR, body + " " + ANCHOR, 1), encoding="utf-8", newline="")
        rc, log = link()
        stub_syms, cand_objs = parse(log)
        cand_objs &= keep
        print("pass %d: rc=%d keep=%d stub-collisions=%d cand-faults=%d"
              % (p, rc, len(keep), len(stub_syms), len(cand_objs)))
        if rc == 0:
            print("CLEAN with %d objects wired" % len(keep))
            if not write:
                MK.write_text(mk_bak, encoding="utf-8", newline="")
                STUB_C.write_text(stub_bak, encoding="utf-8", newline="")
                print("(--write not given: both files restored)")
            return 0
        if stub_syms:
            n = retire(stub_syms)
            print("  retired %d stub definitions" % n)
            if n:
                continue
        if cand_objs:
            keep -= cand_objs
            continue
        print("  stuck: no stub and no candidate blamed")
        break

    MK.write_text(mk_bak, encoding="utf-8", newline="")
    STUB_C.write_text(stub_bak, encoding="utf-8", newline="")
    if STUB_O.exists():
        STUB_O.unlink()
    print("restored objects.mk and infrastructure_stubs.c")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))

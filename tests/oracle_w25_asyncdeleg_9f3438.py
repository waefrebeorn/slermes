#!/usr/bin/env python3
"""Differential oracle for tools/async_delegation_recovery_hints.py (gate w25_asyncdeleg_9f3438).

The live module is imported and driven through its own seams, each one pinned to
the fixture's value:

  * ``transcript_tail`` opens a real file -- the fixture's read window is written
    to a temp file and the live function reads it back (a null window is a path
    that does not exist: the OSError branch);
  * ``transcript_tails`` is the same over a map of indexes;
  * ``git_state_hint`` shells out to git -- ``subprocess.run`` and
    ``noninteractive_repo_git_env`` are pinned to the fixture's three replies, so
    the live formatter builds the line and nothing is spawned;
  * the module's two bounds.

Everything compared is the bounded text the recovery event would carry.
"""
import json
import subprocess
import sys
import tempfile
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_asyncdeleg_9f3438"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools import async_delegation_recovery_hints as live  # noqa: E402

WANTED = {"tail_text", "tails", "git_hint", "constants"}
TMP = Path(tempfile.mkdtemp(prefix="w25asyncdeleg-"))
CWD_OK = TMP / "repo"
CWD_OK.mkdir(parents=True, exist_ok=True)
CWD_MISSING = TMP / "no-such-repo"


def _num(value, default):
    return value if isinstance(value, int) and not isinstance(value, bool) else default


def _write(index, raw):
    """A real transcript file holding the fixture's read window."""
    path = TMP / ("transcript-%s.txt" % index)
    path.write_bytes(raw.encode("utf-8"))
    return path


def tail_text(req, index="0"):
    raw = req.get("raw")
    lines = _num(req.get("lines"), live.TAIL_LINES)
    chars = _num(req.get("chars"), live.TAIL_CHARS)
    if not isinstance(raw, str):
        # A non-path raw value is the unreadable-file seam: nothing to tail.
        return None
    path = _write(index, raw)
    return live.transcript_tail(str(path), lines=lines, chars=chars)


def tails(req):
    contents = req.get("contents")
    paths = {}
    if isinstance(contents, dict):
        for index, raw in contents.items():
            if isinstance(raw, str):
                paths[index] = str(_write("tails-" + index, raw))
            else:
                paths[index] = str(TMP / ("missing-%s.txt" % index))
    return live.transcript_tails(paths)


def git_hint(req):
    head = req.get("head")
    branch = req.get("branch")
    status = req.get("status")
    cwd = req.get("cwd")
    is_dir = req.get("cwd_is_dir")

    def fake_run(argv, **_kwargs):
        args = [str(a) for a in argv]
        if "log" in args:
            value = head
        elif "rev-parse" in args:
            value = branch
        else:
            value = status
        if not isinstance(value, str):
            return types.SimpleNamespace(returncode=1, stdout="")
        return types.SimpleNamespace(returncode=0, stdout=value)

    real_run, real_env = subprocess.run, live.noninteractive_repo_git_env
    subprocess.run = fake_run
    live.noninteractive_repo_git_env = lambda _cwd: {}
    try:
        path = str(CWD_OK if is_dir is True else CWD_MISSING) if isinstance(cwd, str) and cwd else ""
        line = live.git_state_hint(path)
    finally:
        subprocess.run, live.noninteractive_repo_git_env = real_run, real_env
    # The fixture's cwd stands in for a real directory (the isdir seam); map it back.
    if isinstance(line, str) and path:
        return line.replace(path, cwd)
    return line


def constants():
    return {
        "tail_lines": live.TAIL_LINES,
        "tail_chars": live.TAIL_CHARS,
        "git_timeout_s": live._GIT_TIMEOUT_S,
    }


def expected(r):
    f = r.get("func")
    if f == "tail_text":
        return tail_text(r, index=str(r.get("case")))
    if f == "tails":
        return tails(r)
    if f == "git_hint":
        return git_hint(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()

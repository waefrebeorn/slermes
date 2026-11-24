#!/usr/bin/env python3
"""Differential oracle for gateway/session_managed_worker_log.py (gate w25_sessionman_1c48ef).

Three seams are stood in for, never re-implemented:

  * the pipe: a fake stream hands back the fixture chunks (utf-8 encoded) and
    then b"" for EOF;
  * the redactor: ``_redact`` is replaced by identity for the framing cases --
    ``agent.redact.redact_sensitive_text`` is its own module's business;
  * the file: ``_append`` writes into a fresh temp dir, so the rotation
    decision and the bytes actually written are read back off disk instead of
    being recomputed here.

``worker_log_path`` runs unmocked (pure path arithmetic), and the module's
literal sink caps are read off the live module.
"""
import ast
import importlib.util
import json
import logging
import os
import sys
import tempfile
from pathlib import Path

# The append cases run the real ``_append``, which calls
# ``hermes_constants.mkdir_under_hermes_home`` and therefore needs a resolvable
# home.  A bare ``bash -lc`` on Windows exports neither USERPROFILE nor a real
# HOME, so anchor both at the scratch dir before importing anything of ours.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = tempfile.gettempdir()
if not os.environ.get("HOME") or not Path(os.environ["HOME"]).exists():
    os.environ["HOME"] = tempfile.gettempdir()

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_sessionman_1c48ef"
LIVE_PATH = QUARRY / "gateway" / "session_managed_worker_log.py"


def load_live():
    if str(QUARRY) not in sys.path:
        sys.path.insert(0, str(QUARRY))
    spec = importlib.util.spec_from_file_location("_live_session_managed_worker_log", LIVE_PATH)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["_live_session_managed_worker_log"] = mod
    spec.loader.exec_module(mod)
    return mod


live = load_live()

WANTED = {"worker_log_path", "frame_lines", "append_plan", "constants"}


def worker_log_path(req):
    path = live.worker_log_path(req.get("profile_id"))
    return None if path is None else str(path)


class _Stream:
    def __init__(self, chunks):
        self.chunks = [c.encode("utf-8") for c in chunks]
        self.closed = False

    def readline(self, _limit):
        if not self.chunks:
            return b""
        return self.chunks.pop(0)

    def close(self):
        self.closed = True


class _Capture(logging.Handler):
    def __init__(self, records):
        super().__init__()
        self.records = records

    def emit(self, record):
        self.records.append(record.getMessage())


def frame_lines(req):
    plan = req.get("plan")
    if not isinstance(plan, dict):
        return None
    chunks = plan.get("chunks")
    if chunks is not None and not isinstance(chunks, list):
        return None
    chunks = chunks or []
    pid = plan.get("pid", 0)
    fail_from = plan.get("fail_from")

    appended = []

    def fake_append(_path, text):
        appended.append(text)
        if fail_from is not None and len(appended) - 1 >= fail_from:
            raise OSError("No space left on device")

    stream = _Stream(chunks)
    real_append, real_redact = live._append, live._redact
    live._append, live._redact = fake_append, lambda line, _scrub: line
    records = []
    handler = _Capture(records)
    logger = logging.getLogger(live.__name__)
    logger.addHandler(handler)
    old_level, old_propagate = logger.level, logger.propagate
    logger.setLevel(logging.WARNING)
    logger.propagate = False
    try:
        live.drain_worker_stderr(stream, Path(tempfile.gettempdir()) / "managed-worker.log",
                                 pid, lambda: ())
    finally:
        logger.removeHandler(handler)
        logger.setLevel(old_level)
        logger.propagate = old_propagate
        live._append, live._redact = real_append, real_redact
    return {"appended": appended, "warned": bool(records)}


def append_plan(req):
    plan = req.get("plan")
    if not isinstance(plan, dict):
        return None
    size = plan.get("size", 0)
    nbytes = plan.get("nbytes", 0)
    if not isinstance(size, (int, float)) or isinstance(size, bool):
        return None
    if not isinstance(nbytes, (int, float)) or isinstance(nbytes, bool):
        return None
    directory = Path(tempfile.mkdtemp(prefix="w25sm_"))
    target = directory / "managed-worker.log"
    if size:
        target.write_bytes(b"x" * int(size))
    live._append(target, "y" * int(nbytes))
    rotated = (directory / (live.LOG_NAME + ".1")).exists()
    final = target.stat().st_size
    written = final - (0 if rotated else int(size))
    return {"rotate": rotated, "write": written}


def constants():
    source = LIVE_PATH.read_text(encoding="utf-8")
    modes = {n.value for n in ast.walk(ast.parse(source))
             if isinstance(n, ast.Constant) and n.value == 0o600}
    assert modes == {0o600}, modes
    assert "LOG_NAME + '.1'" in source
    return {
        "log_name": live.LOG_NAME,
        "max_log_bytes": live.MAX_LOG_BYTES,
        "max_line_bytes": live.MAX_LINE_BYTES,
        "kept_line_chars": live.KEPT_LINE_CHARS,
        "rotate_suffix": ".1",
        "file_mode": 0o600,
    }


def expected(req):
    f = req.get("func")
    if f == "worker_log_path":
        return worker_log_path(req)
    if f == "frame_lines":
        return frame_lines(req)
    if f == "append_plan":
        return append_plan(req)
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
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    if cases == 0:
        print("ORACLE %s: no cases consumed" % GATE, file=sys.stderr)
        sys.exit(1)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Differential oracle for tools/connectors/gateway/errors.py (gate w21_errors_5bcef1).

The live module is stdlib-only (`tools.connectors.turn` only supplies the three
surface names), so the oracle imports it as-is and calls the real
`parse_gateway_error`, `render_connection_required` and `_default_message`.
An exception class travels as its name plus the attributes a caller reads.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "tools" / "connectors" / "gateway" / "errors.py"
GATE = "w21_errors_5bcef1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("w21_gateway_errors_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["w21_gateway_errors_live"] = live
_spec.loader.exec_module(live)

_RENDER_KEYS = ("connector", "message", "connect_url", "hint", "surface", "code")


def expected_parse(r):
    try:
        exc = live.parse_gateway_error(r.get("status"), r.get("body"), r.get("headers"))
    except Exception as err:  # noqa: BLE001 - malformed headers only
        return {"error_type": type(err).__name__}
    out = {
        "type": type(exc).__name__,
        "message": str(exc),
        "code": exc.code,
        "status": exc.status,
        "request_id": exc.request_id,
        "retryable": exc.retryable,
    }
    if isinstance(exc, live.RateLimited):
        out["retry_after"] = exc.retry_after
    return out


def expected_render(r):
    kwargs = {k: r.get(k) for k in _RENDER_KEYS if k in r}
    return live.render_connection_required(**kwargs)


def expected(r):
    f = r.get("func")
    if f == "parse":
        return expected_parse(r)
    if f == "render":
        return expected_render(r)
    if f == "default_message":
        return live._default_message(r.get("code"), r.get("connector"))
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"parse", "render", "default_message"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
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
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()

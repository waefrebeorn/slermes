import json
"""twin for loops_parse_loop_args.

Traces the LIVE hermes_cli/loops.py::parse_loop_args branch ladder with the
SAME rung order as the C harness:
  B1 empty / B2 --times match / B3 times invalid / B4 --until match /
  B5 every-sugar / B6 interval token / B7 missing prompt.
"""
import os, sys, re, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_INTERVAL_TOKEN_RE = re.compile(r"^(?=\d)(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?$", re.IGNORECASE)


def parse_interval_token(token):
    if not token:
        return None
    m = _INTERVAL_TOKEN_RE.match(token.strip())
    if not m:
        return None
    h, mn, s = (int(g) if g else 0 for g in m.groups())
    total = h * 3600 + mn * 60 + s
    return total if total > 0 else None


def parse_loop_args_trace(text):
    """LIVE parse_loop_args with the cup tracer on the same rungs as C."""
    raw = (text or "").strip()
    result = {"interval_seconds": None, "prompt": "", "times": 0,
              "until": "", "error": None}
    c = Cup("loops_parse_loop_args")

    # B1: empty
    if not raw:
        c.branch(True)
        result["error"] = "empty"
        out = dict(result)
        c.finalize()
        return result, c

    c.branch(False)
    times = 0
    until = ""

    m_times = re.search(r"\s--times\s+(\S+)", raw)
    if m_times:                       # B2
        c.branch(True)
        try:
            times = int(m_times.group(1))
            if times < 1:
                raise ValueError       # B3 invalid
            c.branch(False)
        except ValueError:            # B3
            c.branch(True)
            result["error"] = f"--times expects a positive integer"
            c.finalize()
            return result, c
        raw = (raw[: m_times.start()] + raw[m_times.end():]).strip()
    else:
        c.branch(False)               # B2 no match

    m_until = re.search(r"\s--until\s+(.+)$", raw, re.DOTALL)
    if m_until:                       # B4
        c.branch(True)
        until = m_until.group(1).strip()
        raw = raw[: m_until.start()].strip()
    else:
        c.branch(False)

    tokens = raw.split(None, 1)
    if tokens and tokens[0].lower() == "every" and len(tokens) > 1:   # B5
        c.branch(True)
        raw = tokens[1]
        tokens = raw.split(None, 1)
    else:
        c.branch(False)

    interval = None
    if tokens and parse_interval_token(tokens[0]) is not None:        # B6
        c.branch(True)
        interval = parse_interval_token(tokens[0])
        raw = tokens[1].strip() if len(tokens) > 1 else ""
    else:
        c.branch(False)

    if not raw:                        # B7 missing prompt
        c.branch(True)
        result["error"] = "missing prompt (usage: /loop [interval] <prompt>)"
        out = dict(result)
        c.finalize()
        return result, c

    c.branch(False)
    result["interval_seconds"] = interval
    result["prompt"] = raw
    result["times"] = times
    result["until"] = until
    c.finalize()
    return result, c


def run(inputs):
    return [parse_loop_args_trace(text)[1].finalize() for text in inputs]

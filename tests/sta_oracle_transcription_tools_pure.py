"""Oracle for tools/transcription_tools.py pure helpers.

Tests against LIVE Python source.
"""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from tools.transcription_tools import (
    _is_local_stt_provider,
    _command_stt_env_passthrough,
    _is_local_or_private_url,
    _confidence_thresholds,
    _is_hallucinated_segment,
)


def run(c):
    op = c.get("op")
    if op == "ts_is_local_stt_provider":
        return _is_local_stt_provider(c.get("value", ""), {})
    if op == "ts_is_local_or_private_url":
        return _is_local_or_private_url(c.get("value", ""))
    if op == "ts_command_stt_env_passthrough":
        cfg = c.get("value", {})
        if isinstance(cfg, str):
            cfg = json.loads(cfg)
        result = _command_stt_env_passthrough(cfg)
        if not result:
            return "[]"
        return "[" + ", ".join(result) + "]"
    if op == "ts_confidence_thresholds":
        cfg = c.get("value", {})
        if isinstance(cfg, str):
            cfg = json.loads(cfg)
        nsp, lp = _confidence_thresholds(cfg)
        return f"{nsp:.17g},{lp:.17g}"
    if op == "ts_is_hallucinated_segment":
        seg = c.get("value", {})
        if isinstance(seg, str):
            seg = json.loads(seg)
        nsp = c.get("no_speech_threshold", 0.6)
        lp = c.get("logprob_threshold", -1.0)
        return _is_hallucinated_segment(seg, nsp, lp)
    return None


def main():
    with open(sys.argv[1]) as f:
        cases = json.load(f)
    for c in cases:
        r = run(c)
        if r is None:
            print("none")
        elif isinstance(r, bool):
            print("true" if r else "false")
        elif isinstance(r, (int, float)):
            if isinstance(r, float) and r == int(r):
                print(int(r))
            else:
                print(r)
        elif isinstance(r, str):
            print(r)
        else:
            print(json.dumps(r, sort_keys=True))


if __name__ == "__main__":
    main()

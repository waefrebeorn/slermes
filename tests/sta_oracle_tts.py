#!/usr/bin/env python3
"""
sta_oracle_tts.py — oracle for t_port_tts.c.

Recomputes each case from the LIVE agent/tts_provider.py:resolve_output_format
and emits one JSON line per case (same shape as the C harness). The runner
diffs them byte-for-byte.
"""
import json
import sys
import importlib.util


def _load():
    for base in sys.path:
        cand = f"{base}/agent/tts_provider.py"
        try:
            spec = importlib.util.spec_from_file_location("live_tts", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import agent.tts_provider as mod  # type: ignore
    return mod


tp = _load()


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_tts.py <values.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        values = json.load(f)
    for value in values:
        out = tp.resolve_output_format(value)
        rec = {"value": value if isinstance(value, str) else "", "out": out}
        sys.stdout.write(json.dumps(rec, ensure_ascii=False, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

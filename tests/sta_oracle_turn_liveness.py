#!/usr/bin/env python3
"""
sta_oracle_turn_liveness.py — Oracle for the pure settings half of
agent/turn_liveness.py (C: src/agent/port_turn_liveness.c).

Replays the LIVE module over each config document, counting the warnings it
emits via a log handler, and diffs (timeout, poll, warning-count) plus the
formatted warning body.
"""
import json
import logging
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import agent.turn_liveness as TL  # noqa: E402


class Counter(logging.Handler):
    def __init__(self):
        super().__init__()
        self.messages = []

    def emit(self, record):
        self.messages.append(record.getMessage())


def replay(config_json):
    cfg = None if config_json is None else json.loads(config_json)
    handler = Counter()
    TL.logger.addHandler(handler)
    old_level = TL.logger.level
    TL.logger.setLevel(logging.DEBUG)
    try:
        timeout, poll = TL.resolve_turn_liveness_settings(cfg)
    finally:
        TL.logger.removeHandler(handler)
        TL.logger.setLevel(old_level)
    return timeout, poll, handler.messages


def main() -> int:
    # The C harness emits UTF-8 (the warning text contains an em dash); the
    # default Windows codec would mojibake it and turn a match into a diff.
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE turn_liveness: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        if "msg" in c:
            raw = c["raw"]
            want = "Invalid %s in config.yaml: %r \u2014 falling back to default %.1f." % (
                c["key"], raw, c["def"])
            if c["msg"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}:\n  C ={c['msg']!r}\n  PY={want!r}")
            continue

        timeout, poll, messages = replay(c["config"])
        want_has = 0 if timeout is None else 1
        want_timeout = 0.0 if timeout is None else float(timeout)
        if abs(want_timeout - c["timeout"]) > 1e-9:
            mism += 1
            print(f"MISMATCH {c['case']}.timeout: C={c['timeout']} PY={want_timeout}")
        if abs(float(poll) - c["poll"]) > 1e-9:
            mism += 1
            print(f"MISMATCH {c['case']}.poll: C={c['poll']} PY={poll}")
        if c["has_timeout"] != want_has:
            mism += 1
            print(f"MISMATCH {c['case']}.has_timeout: C={c['has_timeout']} PY={want_has}")
        if c["warnings"] != len(messages):
            mism += 1
            print(f"MISMATCH {c['case']}.warnings: C={c['warnings']} PY={len(messages)} {messages}")

    print(f"ORACLE turn_liveness: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

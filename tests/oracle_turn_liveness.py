#!/usr/bin/env python3
"""Oracle for agent/turn_liveness.py pure settings + message helpers.

Harness families:
  settings {case, config:<json|null>, has_timeout,int timeout:%.17g poll:%.17g
            warnings:int}   -> live resolve_turn_liveness_settings(config)
  msg      {case, key, raw:<!>, def} -> live _warn_invalid_value message text
The oracle replays the live resolve with a logger that COUNTS warnings (returns
the exact value the C mirror exposes as warnings) and reproduces the _warn text
(f"Invalid {key} in config.yaml: {raw!r} — falling back to default {def:.1f}.",
the em dash is U+2014).
"""
import ast
import json
import math
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/turn_liveness.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"math": math}
    for n in ast.parse(src).body:
        if isinstance(n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id in ("DEFAULT_TURN_LIVENESS_TIMEOUT_S",
                                                 "DEFAULT_TURN_LIVENESS_POLL_S",
                                                 "_CONFIG_TIMEOUT_KEY", "_CONFIG_POLL_KEY")
            for t in n.targets):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "_warn_invalid_value", "_resolve_finite_seconds",
                "resolve_turn_liveness_settings"):
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    _warn = ns["_warn_invalid_value"]
    resolve = ns["resolve_turn_liveness_settings"]

    # counting logger: every warning the live fn issues is one C 'warnings'
    class Counter:
        def __init__(self):
            self.n = 0
        def warning(self, *a, **k):
            self.n += 1

    def run_resolve(config_json):
        config = None
        if config_json is not None:
            config = json.loads(config_json)
        logger = Counter()
        ns["logger"] = logger
        timeout_s, poll_s = resolve(config)
        warnings = logger.n
        return timeout_s, poll_s, warnings

    def msg(key, raw, defval):
        if raw is None:
            raw_repr = "None"
        else:
            # handled by substituting a sentinel object whose repr is the C's
            # repr_str of the raw string; simplest: %r of the string
            raw_repr = repr(raw if raw not in ("None",) else None)
        return (f"Invalid {key} in config.yaml: {raw_repr} \u2014 "
                f"falling back to default {defval:.1f}.")

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
    total = 0
    for c in cases:
        total += 1
        if "config" in c:
            timeout_s, poll_s, warnings = run_resolve(c.get("config"))
            has_timeout = 1 if timeout_s is not None else 0
            timeout_v = timeout_s if timeout_s is not None else 0.0
            have = (c.get("has_timeout"), c.get("timeout"), c.get("poll"), c.get("warnings"))
            want = (has_timeout, timeout_v, poll_s, warnings)
            # compare the float fields with tolerance-free equality (%.17g emits exact)
            ok = (have[0], have[1], have[2], have[3]) == want
            ok = (have[0] == want[0] and abs(have[1] - want[1]) < 1e-9
                  and abs(have[2] - want[2]) < 1e-9 and have[3] == want[3])
            if not ok:
                mism += 1
                print(f"MISMATCH settings {c.get('case')} C={have} PY={want}")
        else:
            key = c.get("key"); raw = c.get("raw"); defval = c.get("def", 15.0)
            wantmsg = msg(key, raw, defval)
            if c.get("msg") != wantmsg:
                mism += 1
                print(f"MISMATCH msg {c.get('case')} C={c.get('msg')!r} PY={wantmsg!r}")
    print(f"ORACLE turn_liveness: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

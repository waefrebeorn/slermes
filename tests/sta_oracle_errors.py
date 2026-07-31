#!/usr/bin/env python3
"""
sta_oracle_errors.py — oracle for t_port_errors.c.

Enumerates the LIVE agent.errors module's three domain exception classes and
emits one JSON line per class, same shape as the C harness:
    {"name": <Exception.__name__>, "base": <int>}
where base maps the Python base class to an int tag:
    0 Exception, 1 RuntimeError, 2 ValueError
(mirroring t_port_errors.c).

The runner diffs the two; any missing/renamed domain error in the C side is
caught. Deterministic LIVE-Python resolution prefers the canonical dev repo
(parent of slermes/) over any installed/stale copy on sys.path, which would
manufacture false FAPs.
"""

import sys
import os
import importlib.util
import json as _json

BASE = {
    "Exception": 0,
    "RuntimeError": 1,
    "ValueError": 2,
}

# The three domain exception classes defined in agent/errors.py.
CLASSES = ["SSLConfigurationError", "EmptyStreamError", "MoAPresetNotFoundError"]


def _load_errors():
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    for base in sys.path:
        cand = os.path.join(base, "agent", "errors.py")
        if os.path.exists(cand):
            try:
                spec = importlib.util.spec_from_file_location("live_errors", cand)
                mod = importlib.util.module_from_spec(spec)
                sys.modules["live_errors"] = mod
                spec.loader.exec_module(mod)
                return mod
            except Exception:
                continue
    import agent.errors as mod  # type: ignore
    return mod


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_errors.py <unused>\n")
        return 2
    mod = _load_errors()
    for name in CLASSES:
        cls = getattr(mod, name, None)
        if cls is None:
            # Emit nothing for a missing class so the diff flags it as absent.
            continue
        base_name = cls.__bases__[0].__name__ if cls.__bases__ else "Exception"
        base = BASE.get(base_name, 0)
        sys.stdout.write(_json.dumps({"name": name, "base": base}) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Differential oracle for gate w16_credential_c298e3 (agent/credential_pool_errors.py).

Loads the LIVE module via spec_from_file_location. The delegated pattern redactor
(agent.redact.redact_sensitive_text) and reset_delay_from_message are pinned to
identity/None so the module's own known-secret masking and context normalisation
are compared directly (avoids wall-clock time.time() in the fallback branch).
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "agent" / "credential_pool_errors.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
for _pkg in ("agent", "hermes_cli"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [str(Path(QUARRY) / _pkg)]
        sys.modules[_pkg] = _m

_SPEC = importlib.util.spec_from_file_location(
    "w16_credential_errors_live", str(Path(QUARRY) / "agent" / "credential_pool_errors.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)

# Explicit seams for the two delegated helpers.
mod.redact_sensitive_text = lambda text, **kw: text
mod.reset_delay_from_message = lambda message: None


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, list): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def expected(rec):
    f = rec.get("func")
    secrets = tuple(rec.get("known_secrets") or ())
    if f == "parse_absolute_timestamp":
        return mod._parse_absolute_timestamp(rec.get("value"))
    if f == "redact_error_text":
        return mod.redact_error_text(rec.get("text") or "", secrets)
    if f == "normalize_error_context":
        return mod._normalize_error_context(rec.get("error_context"), secrets)
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w16_credential_c298e3: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()

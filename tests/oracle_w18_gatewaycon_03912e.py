#!/usr/bin/env python3
"""Differential oracle for gate w18_gatewaycon_03912e (hermes_cli/gateway_connections.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "gateway_connections.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = [str(Path(QUARRY) / "hermes_cli")]
sys.modules["hermes_cli"] = _pkg

_SPEC = importlib.util.spec_from_file_location(
    "w18_gatewaycon_live", str(Path(QUARRY) / "hermes_cli" / "gateway_connections.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _canon(x):
    if x is None:
        return ("null",)
    if isinstance(x, bool):
        return ("num", 1 if x else 0)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if isinstance(x, str):
        return ("str", x)
    if isinstance(x, list):
        return ("list", [_canon(v) for v in x])
    if isinstance(x, dict):
        return ("obj", sorted((str(k), _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def expected(rec):
    f = rec.get("func")
    if f == "revision":
        return mod._revision(rec.get("doc"))
    if f == "plain":
        return mod._plain(rec.get("secret"))
    if f == "envelope":
        return mod._envelope(rec.get("value"))
    if f == "token_preview":
        return mod._token_preview(rec.get("value"))
    if f == "kept_envelope":
        return mod._kept_envelope(rec.get("token"), rec.get("existing"))
    if f == "public_connection":
        return mod._public_connection(rec.get("entry"))
    if f == "public_registry":
        return mod.public_registry(rec.get("reg"))
    if f == "signed_in_ids_gone":
        return mod._signed_in_ids_gone(rec.get("before"), rec.get("after"))
    if f == "incoming_headers":
        return mod._incoming_headers(rec.get("raw"), rec.get("existing"))
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        want = expected(rec)
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" % (
                    rec.get("func"), json.dumps(got, ensure_ascii=False),
                    json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w18_gatewaycon_03912e: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()

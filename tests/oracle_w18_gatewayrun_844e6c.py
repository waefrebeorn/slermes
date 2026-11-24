#!/usr/bin/env python3
"""Differential oracle for gate w18_gatewayrun_844e6c (hermes_cli/gateway_runtime_cli.py)."""
import importlib.util
import json
import sys
import types
from dataclasses import dataclass
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "gateway_runtime_cli.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)

_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = [str(Path(QUARRY) / "hermes_cli")]
sys.modules["hermes_cli"] = _pkg
CTX = {}
_vi = types.ModuleType("hermes_cli.version_info")
_vi.get_code_identity = lambda: {"sha": CTX.get("code_sha")}
sys.modules["hermes_cli.version_info"] = _vi

_SPEC = importlib.util.spec_from_file_location(
    "w18_gatewayrun_live", str(Path(QUARRY) / "hermes_cli" / "gateway_runtime_cli.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


@dataclass
class Endpoint:
    url: object
    protocol: object
    capabilities: object


@dataclass
class Verdict:
    state: object
    reason_code: object
    detail: object
    endpoint: object


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("num", 1 if x else 0)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((str(k), _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def _verdict(obj):
    ep = obj.get("endpoint") if isinstance(obj, dict) else None
    endpoint = Endpoint(url=ep.get("url"), protocol=ep.get("protocol"),
                        capabilities=ep.get("capabilities")) if isinstance(ep, dict) else None
    return Verdict(state=obj.get("state"), reason_code=obj.get("reason_code"),
                   detail=obj.get("detail"), endpoint=endpoint)


def expected(rec):
    f = rec.get("func")
    if f == "ensure_exit_code":
        return mod.ensure_exit_code(_verdict({"state": rec.get("state"),
                                              "reason_code": rec.get("reason_code")}))
    if f == "verdict_payload":
        CTX["code_sha"] = rec.get("code_sha")
        return mod._verdict_payload(_verdict(rec.get("result") or {}))
    if f == "constants":
        return mod.INVALID_INVOCATION
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
                print("MISMATCH %s case=%s got=%r want=%r" %
                      (rec.get("func"), rec.get("case"), got, want), file=sys.stderr)
    print("ORACLE w18_gatewayrun_844e6c: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()

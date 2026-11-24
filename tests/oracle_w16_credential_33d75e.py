#!/usr/bin/env python3
"""Differential oracle for gate w16_credential_33d75e (agent/credential_pool_plugin.py).

Loads the LIVE module via spec_from_file_location with a stubbed hermes_cli.auth
for _OAUTH_GRANT_DEAD_CODES. Drives the three deterministic helpers directly;
the clock is injected for plugin_row_is_expiring (Python reads time.time()).
"""
import dataclasses
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "agent" / "credential_pool_plugin.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
for _pkg in ("agent", "hermes_cli"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [str(Path(QUARRY) / _pkg)]
        sys.modules[_pkg] = _m

_fake_auth = types.ModuleType("hermes_cli.auth")
_fake_auth._OAUTH_GRANT_DEAD_CODES = frozenset({"invalid_grant", "invalid_token", "refresh_token_reused"})
sys.modules["hermes_cli.auth"] = _fake_auth

_SPEC = importlib.util.spec_from_file_location(
    "w16_credential_plugin_live", str(Path(QUARRY) / "agent" / "credential_pool_plugin.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


@dataclasses.dataclass
class PC:
    provider: object = ""
    access_token: object = ""
    refresh_token: object = ""
    expires_at_ms: object = None
    extra: object = dataclasses.field(default_factory=dict)
    label: object = ""
    id: object = ""


def _entry(d):
    base = {"provider": "", "access_token": "", "refresh_token": "", "expires_at_ms": None,
            "extra": {}, "label": "", "id": ""}
    base.update(d or {})
    return PC(**{k: base[k] for k in ("provider", "access_token", "refresh_token",
                                     "expires_at_ms", "extra", "label", "id")})


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
    if f == "is_terminal_plugin_refresh_error":
        exc = rec.get("exc") or {}
        if exc.get("kind") == "auth":
            err = mod.AuthError("x", code=exc.get("code"),
                                relogin_required=bool(exc.get("relogin_required")))
        else:
            err = RuntimeError("x")
        return mod.is_terminal_plugin_refresh_error(err)
    if f == "plugin_row_is_expiring":
        now_ms = int(rec.get("now_ms") or 0)
        mod.time = types.SimpleNamespace(time=lambda: now_ms / 1000.0)
        exp = rec.get("expires_at_ms")
        entry = types.SimpleNamespace(expires_at_ms=(int(exp) if exp not in (None, "") else None))
        return mod.plugin_row_is_expiring(entry)
    if f == "apply_plugin_refresh_result":
        out = mod.apply_plugin_refresh_result(_entry(rec.get("entry")), rec.get("result") or {})
        return dataclasses.asdict(out)
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
    print("ORACLE w16_credential_33d75e: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()

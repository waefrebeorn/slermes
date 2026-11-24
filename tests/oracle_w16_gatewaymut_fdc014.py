#!/usr/bin/env python3
"""Differential oracle for hermes_cli/gateway_mutations.py (gate w16_gatewaymut_fdc014)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w16_gatewaymut_fdc014"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


class _GatewayClientError(Exception):
    pass


# Only top-level dependency is gateway_client's error class and timeout.
_seed("hermes_cli")
_seed("hermes_cli.gateway_client", GatewayClientError=_GatewayClientError,
      COMPRESS_RPC_TIMEOUT=120.0)
# confirm_choice late-imports the alias table from cli_modal_mixin.
_CONFIRM_ALIASES = {
    "1": "once", "once": "once", "approve": "once", "yes": "once", "y": "once", "ok": "once",
    "2": "always", "always": "always", "remember": "always",
    "3": "cancel", "cancel": "cancel", "nevermind": "cancel", "no": "cancel", "n": "cancel",
}
_seed("hermes_cli.cli_modal_mixin", _CONFIRM_ALIASES=_CONFIRM_ALIASES)

SRC = QUARRY + "/hermes_cli/gateway_mutations.py"
_spec = importlib.util.spec_from_file_location("gateway_mutations_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["gateway_mutations_live"] = live
_spec.loader.exec_module(live)


def _caught(fn):
    try:
        return fn()
    except Exception as exc:
        return {"__raise__": type(exc).__name__, "msg": str(exc)}


def live_out(rec):
    func = rec.get("func")
    if func == "refusal":
        return live.model_refusal_text(rec.get("refusal") or {},
                                       confirmed=bool(rec.get("confirmed", False)))
    if func == "title":
        return live.confirmation_title(rec.get("refusal") or {})
    if func == "choice":
        choices = [tuple(c) for c in (rec.get("choices") or [])]
        return live.confirm_choice(rec.get("raw"), choices)
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("refusal", "title", "choice"):
            continue
        cases += 1
        py = _caught(lambda: live_out(rec))
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

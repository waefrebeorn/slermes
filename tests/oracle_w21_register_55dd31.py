#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/qqbot/register.py
(gate w21_register_55dd31).

Seams stubbed (nothing else):
  * `gateway.platforms._shared` -- only to avoid the gateway package __init__
    (it imports the mautrix-era adapter chain, which needs a real HERMES home).
    `env_is_connected` / `config_connected_extra` are EXECUTED from the live
    source via AST extraction, not reimplemented.
  * `hermes_cli.gateway.get_env_value` -- the profile/.env reader; the fixture
    supplies the resolved values per case (a config seam).
  * `.adapter` (QQAdapter/check_qq_requirements) -- the mautrix client itself.
  * `.gateway_env` -- the gateway-config bridge (ENV_OVERRIDES/AUTHZ), not part
    of this module's declarative surface.
`register.py` itself is loaded by path, so `is_connected`, `_PLATFORM_HINT`,
`standalone_send`/`setup_qqbot` and the metadata `register()` declares are the
live implementations.
"""
import ast
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w21_register_55dd31"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_ANN = 0x00  # annotations are strings in these modules (from __future__ import annotations)
for _flag in (__import__("__future__").annotations.compiler_flag,):
    _ANN = _flag


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


def _exec_funcs(path, names, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec",
                         flags=_ANN, dont_inherit=True), into)


# --- gateway.platforms shell + real _shared helpers ------------------------
for _pkg in ("gateway", "gateway.platforms"):
    _m = types.ModuleType(_pkg)
    _m.__path__ = []
    sys.modules.setdefault(_pkg, _m)

_shared = types.ModuleType("gateway.platforms._shared")
_shared.Any = object
_shared.Callable = object
_exec_funcs(QUARRY / "gateway" / "platforms" / "_shared.py",
            {"env_is_connected", "config_connected_extra"}, _shared.__dict__)
sys.modules["gateway.platforms._shared"] = _shared
sys.modules["gateway"].platforms = sys.modules["gateway.platforms"]
sys.modules["gateway.platforms"]._shared = _shared

_send_targets = _load("gateway.platforms.send_targets",
                      QUARRY / "gateway" / "platforms" / "send_targets.py")
sys.modules["gateway.platforms"].send_targets = _send_targets

# --- hermes_cli.gateway (env reader) ---------------------------------------
_env_map = {}
_hermes_cli = types.ModuleType("hermes_cli")
_hermes_cli.__path__ = []
_hg = types.ModuleType("hermes_cli.gateway")
_hg.get_env_value = lambda name: _env_map.get(name)
_hermes_cli.gateway = _hg
sys.modules["hermes_cli"] = _hermes_cli
sys.modules["hermes_cli.gateway"] = _hg

# --- qqbot package shell ---------------------------------------------------
_pkg = types.ModuleType("qqpkg")
_pkg.__path__ = [str(QUARRY / "plugins" / "platforms" / "qqbot")]
sys.modules["qqpkg"] = _pkg

_adapter = types.ModuleType("qqpkg.adapter")
_adapter.QQAdapter = type("QQAdapter", (), {})
_adapter.check_qq_requirements = lambda pconfig: True
sys.modules["qqpkg.adapter"] = _adapter

_gwenv = types.ModuleType("qqpkg.gateway_env")
_gwenv.ENV_OVERRIDES = ()
_gwenv.ENV_ENABLE_CREDENTIALS = ("QQ_APP_ID", "QQ_CLIENT_SECRET")
_gwenv.AUTHZ = {}
sys.modules["qqpkg.gateway_env"] = _gwenv

live = _load("qqpkg.register", QUARRY / "plugins" / "platforms" / "qqbot" / "register.py")
_pkg.adapter = _adapter
_pkg.gateway_env = _gwenv

POLICY_FIELDS = ("label", "media", "captionable", "text_direct")


class _Cfg:
    """Minimal PlatformConfig stand-in (only the attributes the module reads)."""
    def __init__(self, extra=None, token=None, api_key=None):
        self.extra = extra
        self.token = token
        self.api_key = api_key


def _recorded_meta():
    class _Ctx:
        kw = None

        def register_platform(self, **kwargs):
            self.kw = kwargs

    ctx = _Ctx()
    live.register(ctx)
    kw = ctx.kw
    meta = {
        "name": kw["name"],
        "label": kw["label"],
        "emoji": kw["emoji"],
        "docs_url": kw["docs_url"],
        "dashboard_description": kw["dashboard_description"],
        "required_env": list(kw["required_env"]),
        "dashboard_env_vars": list(kw["dashboard_env_vars"]),
        "install_hint": kw["install_hint"],
        "cron_deliver_env_var": kw["cron_deliver_env_var"],
        "platform_hint": kw["platform_hint"],
        "standalone_send": {f: getattr(kw["standalone_send"], f) for f in POLICY_FIELDS},
    }
    return meta


def expected(r):
    f = r.get("func")
    if f == "meta":
        return _recorded_meta()
    if f == "hint":
        return live._PLATFORM_HINT
    extra = r.get("extra")
    if f == "is_connected":
        env = r.get("env") if isinstance(r.get("env"), dict) else {}
        _env_map.clear()
        _env_map.update(env)
        return live.is_connected(_Cfg(extra=extra))
    if f == "config_connected":
        conn = sys.modules["gateway.platforms._shared"].config_connected_extra(
            "app_id", "client_secret")
        extra_obj = extra if isinstance(extra, dict) else {}
        return conn(_Cfg(extra=extra_obj, token=r.get("token"), api_key=r.get("api_key")))
    return "UNKNOWN_FUNC"


def main():
    cases = mism = 0
    wanted = {"is_connected", "config_connected", "hint", "meta"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            py = {"status": "error", "type": type(exc).__name__, "message": str(exc)}
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()

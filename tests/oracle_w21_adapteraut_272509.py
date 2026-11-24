#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/discord/adapter_authorization.py
(gate w21_adapteraut_272509).

The live module source is executed. `is_automated_message` is lifted as a
FunctionDef, and the `DiscordAuthorizationMixin` class is imported as-is so
`reauthorize_native_roles` runs its real body (CSV->role-set coercion, the
early returns, the post-I/O policy re-read and the final member predicate)
against a scripted transport. The imports that body performs at call time --
gateway.config (config seam), gateway.run/hermes_constants (profile scope),
plugins.platforms.discord.adapter (the DM guild read) and discord
(HTTPException) -- are config/transport seams and are stubbed in sys.modules.
"""
import ast
import asyncio
import contextlib
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "plugins" / "platforms" / "discord" / "adapter_authorization.py"
GATE = "w21_adapteraut_272509"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("discord_adapter_authorization_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["discord_adapter_authorization_live"] = live
_spec.loader.exec_module(live)

# --- live helper lifted straight out of the module source -------------------
_lifted = {"__name__": "adapter_authorization_helpers"}
_tree = ast.parse(SRC.read_text(encoding="utf-8"))
for _node in ast.walk(_tree):
    if isinstance(_node, ast.FunctionDef) and _node.name == "is_automated_message":
        exec(compile(ast.Module(body=[_node], type_ignores=[]), str(SRC), "exec"),
             _lifted)

# --- seams: config, transport, exceptions -----------------------------------

class HTTPException(Exception):
    pass


class Platform:
    DISCORD = "discord"


class PlatformConfig:
    pass


class _Cfg:
    platforms = {}


def load_gateway_config():
    return _Cfg()


@contextlib.contextmanager
def _profile_runtime_scope(*_a, **_k):
    yield


def stub(name, **attrs):
    mod = types.ModuleType(name)
    mod.__dict__.update(attrs)
    sys.modules[name] = mod
    return mod


for _pkg in ("gateway", "gateway.platforms", "plugins", "plugins.platforms",
             "plugins.platforms.discord"):
    if _pkg not in sys.modules:
        stub(_pkg)

stub("gateway.config", Platform=Platform, PlatformConfig=PlatformConfig,
     load_gateway_config=load_gateway_config, GatewayConfig=object,
     HomeChannel=object)
stub("gateway.run", _profile_runtime_scope=_profile_runtime_scope)
stub("hermes_constants", get_hermes_home=lambda: "/tmp/hermes-home")
stub("discord", HTTPException=HTTPException)
stub("plugins.platforms.discord.adapter",
     _read_dm_role_auth_guild=lambda: None)   # rebound per case below


def set_dm_guild(value):
    sys.modules["plugins.platforms.discord.adapter"]._read_dm_role_auth_guild = lambda: value


# --- scripted transport -----------------------------------------------------

def ns(value):
    """The decoded message/object shape as Python objects."""
    if isinstance(value, dict):
        return types.SimpleNamespace(**{k: ns(v) for k, v in value.items()})
    if isinstance(value, list):
        return [ns(v) for v in value]
    return value


class FakeGuild:
    def __init__(self, owner):
        self.owner = owner
        self.id = owner.case.get("guild_id_seen", 0)

    async def fetch_member(self, uid):
        owner = self.owner
        owner.stage = 1                       # the .env edit lands during I/O
        if owner.case.get("client_replaced"):
            owner._client = FakeClient(owner)
        err = owner.case.get("fetch_error")
        if err:
            raise {"HTTPException": HTTPException,
                   "TimeoutError": TimeoutError,
                   "OSError": OSError}[err]("scripted transport error")
        return owner.member()


class FakeClient:
    def __init__(self, owner):
        self.owner = owner

    def get_guild(self, guild_id):
        if self.owner.case.get("get_guild_raises"):
            raise ValueError("scripted guild lookup failure")
        if not self.owner.case.get("guild_found", True):
            return None
        return FakeGuild(self.owner)


class Probe(live.DiscordAuthorizationMixin):
    def __init__(self, case):
        self.case = case
        self.stage = 0
        self._client = FakeClient(self) if case.get("client", True) else None

    def member(self):
        raw = self.case.get("member") or {}
        obj = ns(raw)
        obj.guild = types.SimpleNamespace(id=raw.get("guild_id"))
        obj.roles = [types.SimpleNamespace(id=r) for r in (raw.get("role_ids") or [])]
        return obj

    def _gate_raw(self, key, env_name, config=None):
        items = self.case.get("role_items") or []
        if self.stage and "role_items_2" in self.case:
            items = self.case.get("role_items_2") or []
        return list(items)

    def _gate_csv_set(self, raw):
        return list(raw or [])


def expected(r):
    if r.get("func") == "automated":
        return bool(_lifted["is_automated_message"](ns(r.get("message"))))
    case = r
    source = types.SimpleNamespace(chat_type=case.get("chat_type", "guild"),
                                   scope_id=case.get("scope_id"),
                                   user_id=case.get("user_id"))
    set_dm_guild(case.get("dm_guild_id"))
    return bool(asyncio.run(Probe(case).reauthorize_native_roles(source)))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in {"automated", "reauthorize"}:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - a live raise is a harness bug
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()

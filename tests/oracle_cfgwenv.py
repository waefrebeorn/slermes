#!/usr/bin/env python3
"""Differential oracle for gateway/config_env.py pure helpers (gate cfgwenv).

Loads the LIVE gateway/config_env.py via importlib with stubbed gateway.config,
gateway.platform_env and utils modules (the real import chain needs `ruamel`,
absent here). The three pure helpers _only_keys, _platform_layers and
_platform_rank are driven with scripted data; their live results are compared
with the C harness's `out`. A scripted Platform class supplies the enum member
order for _platform_rank.

Input (harness output, one JSON per line):
    {"func":"only_keys"|"platform_layers"|"platform_rank", ..., "out":...}
Summary: ORACLE cfgwenv: N cases, M mismatches.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
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


try:
    import gateway as _gateway  # noqa: F401
except Exception:
    _gateway = _seed("gateway")


class Platform:
    """Minimal stand-in: __members__ is re-seeded per case for _platform_rank."""
    RELAY = "relay"
    __members__ = {}

    def __new__(cls, value):
        if value not in cls.__members__:
            raise ValueError(value)
        return value


_cfg = _seed(
    "gateway.config",
    Platform=Platform,
    _BUILTIN_PLATFORM_VALUES=set(),
    _discovered_registry_entry=lambda x: None,
    GatewayConfig=object,
    HomeChannel=object,
    PlatformConfig=object,
    _getenv_str=lambda k: "",
    _has_usable_api_server_key=lambda k: False,
    SHARED_LISTENER_MIRROR_PLATFORMS=set(),
)
_seed("gateway.platform_env", INT=int, csv_extras=lambda *a: None, env_extras=lambda *a: None)
_seed("utils", is_truthy_value=lambda x: False)

SRC = QUARRY + "/gateway/config_env.py"
_spec = importlib.util.spec_from_file_location("cfgwenv_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["cfgwenv_live"] = live
_spec.loader.exec_module(live)


def canon(x):
    if isinstance(x, (list, tuple)):
        return [canon(v) for v in x]
    if isinstance(x, dict):
        return {k: canon(v) for k, v in x.items()}
    if isinstance(x, float) and x.is_integer():
        return int(x)
    return x


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
        func = rec.get("func")
        names = rec.get("names") or []
        if func == "only_keys":
            py = live._only_keys(rec.get("section"), set(names))
        elif func == "platform_layers":
            py = live._platform_layers(rec.get("yaml_cfg", {}), set(names))
            py = list(py)
        elif func == "platform_rank":
            members = rec.get("members") or []
            Platform.__members__ = {m: m for m in members}
            py = live._platform_rank(types.SimpleNamespace(name=rec.get("name")))
        else:
            continue
        cases += 1
        want = canon(py)
        got = canon(rec.get("out"))
        if want != got:
            mism += 1
            print(f"MISMATCH cfgwenv case={rec.get('case')} func={func} "
                  f"C={got!r} PY={want!r}", file=sys.stderr)
    if cases == 0:
        print("ORACLE cfgwenv: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE cfgwenv: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

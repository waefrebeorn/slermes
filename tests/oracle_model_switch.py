#!/usr/bin/env python3
"""Oracle for the pure slice of hermes_cli/model_switch.py."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# model_switch imports hermes_cli.providers -> utils -> hermes_yaml -> ruamel,
# which is not installed here. Stub the third-party layer so the comparison is
# about the pure functions, not the YAML stack.
import types as _t                                          # noqa: E402
for _name in ("ruamel", "ruamel.yaml"):
    if _name not in sys.modules:
        _m = _t.ModuleType(_name)
        sys.modules[_name] = _m
sys.modules["ruamel"].yaml = sys.modules["ruamel.yaml"]


class _YAML:                                              # pragma: no cover
    def __init__(self, *a, **k):
        raise RuntimeError("oracle stub: ruamel.yaml is not available")


sys.modules["ruamel.yaml"].YAML = _YAML

_err = _t.ModuleType("ruamel.yaml.error")


class YAMLError(Exception):
    pass


_err.YAMLError = YAMLError
sys.modules["ruamel.yaml"].error = _err
sys.modules["ruamel.yaml.error"] = _err

# Any other ruamel.yaml submodule hermes_yaml pulls in gets an empty stub.
for _sub in ("resolver", "parser", "composer", "constructor", "representer",
             "serializer", "emitter", "scanner", "loader", "dumper", "tokens",
             "events", "nodes", "cyaml", "main", "comments", "docinfo",
             "scalarstring", "scalarint", "scalarfloat", "scalarbool",
             "timestamp", "util", "compat", "configobj", "cyaml", "loader",
             "dumper", "reader", "writer"):
    _full = "ruamel.yaml." + _sub
    if _full not in sys.modules:
        _sm = _t.ModuleType(_full)
        setattr(sys.modules["ruamel.yaml"], _sub, _sm)
        sys.modules[_full] = _sm


class VersionedResolver:                                  # pragma: no cover
    pass


sys.modules["ruamel.yaml.resolver"].VersionedResolver = VersionedResolver

# agent.models_dev does `import requests` and only calls requests.get() at
# runtime; the pure functions never reach it.
_req = _t.ModuleType("requests")


class _Resp:                                              # pragma: no cover
    status_code = 500
    text = ""
    content = b""


def _get(*a, **k):                                        # pragma: no cover
    return _Resp()


_req.get = _get
_req.RequestException = type("RequestException", (Exception,), {})
sys.modules.setdefault("requests", _req)

from hermes_cli import model_switch as ms     # noqa: E402


def eq(a, b):
    """Structural compare that treats 1 and 1.0 as equal (JSON round-trip)."""
    if isinstance(a, bool) or isinstance(b, bool):
        return a is b
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return float(a) == float(b)
    if isinstance(a, str) and isinstance(b, str):
        return a == b
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(eq(x, y) for x, y in zip(a, b))
    return a == b


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "clean":
                want = ms._clean(i["value"])
            elif fn == "declared_item_id":
                want = ms._declared_item_id(i["item"])
            elif fn == "declared_model_ids":
                want = ms._declared_model_ids(i["value"])
            elif fn == "entry_models_discovered":
                want = ms._entry_models_discovered(i["entry"])
            elif fn == "models_config_is_allowlist":
                want = ms._models_config_is_allowlist(i["value"],
                                                      bool(i.get("discovered")))
            elif fn == "format_model_for_display":
                want = ms.format_model_for_display(i["model_name"])
            elif fn == "is_nous_hermes_non_agentic":
                want = ms.is_nous_hermes_non_agentic(i["model_name"])
            elif fn == "check_hermes_model_warning":
                want = ms._check_hermes_model_warning(i["model_name"])
            elif fn == "split_version_suffix":
                nums, suffix = ms._split_version_suffix(i["rest"])
                want = [nums, suffix]
            elif fn == "model_sort_key":
                want = list(ms._model_sort_key(i["model_id"], i["prefix"]))
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        if isinstance(raw, dict) and set(raw) == {"str"}:
            raw = raw["str"]
        if not eq(raw, want):
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:240]} "
                  f"PY={want!r}"[:400])
    if total == 0:
        print("ORACLE model_switch: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE model_switch: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

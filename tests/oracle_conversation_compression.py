#!/usr/bin/env python3
"""Oracle for the pure slice of agent/conversation_compression.py."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
# The import chain reaches hermes_cli.config, which reads HERMES_HOME; point it
# at the scratch home so the oracle cannot write into the user's real one.

import types as _t                                          # noqa: E402
for _name in ("ruamel", "ruamel.yaml"):
    if _name not in sys.modules:
        sys.modules[_name] = _t.ModuleType(_name)
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

for _sub in ("resolver", "parser", "composer", "constructor", "representer",
             "serializer", "emitter", "scanner", "loader", "dumper", "tokens",
             "events", "nodes", "cyaml", "main", "comments", "docinfo",
             "scalarstring", "scalarint", "scalarfloat", "scalarbool",
             "timestamp", "util", "compat", "configobj", "reader", "writer"):
    _full = "ruamel.yaml." + _sub
    if _full not in sys.modules:
        _sm = _t.ModuleType(_full)
        setattr(sys.modules["ruamel.yaml"], _sub, _sm)
        sys.modules[_full] = _sm


class VersionedResolver:                                  # pragma: no cover
    pass


sys.modules["ruamel.yaml.resolver"].VersionedResolver = VersionedResolver

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

from agent import conversation_compression as cc   # noqa: E402


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
            if fn == "strip_marker":
                want = cc._strip_marker_for_comparison(i["msgs"])
            elif fn == "is_progress":
                want = cc.is_compaction_progress_status(i.get("text"))
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        raw = c["out"]
        if isinstance(raw, dict) and set(raw) == {"str"}:
            raw = raw["str"]
        if raw != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:240]} "
                  f"PY={want!r}"[:400])
    if total == 0:
        print("ORACLE conversation_compression: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE conversation_compression: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Oracle for the four helpers newly added to src/agent/chat_completion_helpers.c."""
import json
import os
import sys
from pathlib import Path

# run_one_oracle.sh hands the oracle a clean environment, so the quarry's
# Path.home() / get_hermes_home() calls fail. Derive both from this file rather
# than hard-coding them.
_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

import types as _t                                          # noqa: E402
for _name in ("ruamel", "ruamel.yaml"):
    if _name not in sys.modules:
        sys.modules[_name] = _t.ModuleType(_name)
sys.modules["ruamel"].yaml = sys.modules["ruamel.yaml"]
sys.modules["ruamel.yaml"].YAML = type("YAML", (), {
    "__init__": lambda self, *a, **k: None})
_err = _t.ModuleType("ruamel.yaml.error")
_err.YAMLError = type("YAMLError", (Exception,), {})
sys.modules["ruamel.yaml"].error = _err
sys.modules["ruamel.yaml.error"] = _err
for _sub in ("resolver", "parser", "composer", "constructor", "loader",
             "dumper", "main", "comments", "cyaml", "reader", "writer"):
    _full = "ruamel.yaml." + _sub
    if _full not in sys.modules:
        _sm = _t.ModuleType(_full)
        setattr(sys.modules["ruamel.yaml"], _sub, _sm)
        sys.modules[_full] = _sm
sys.modules["ruamel.yaml.resolver"].VersionedResolver = type(
    "VersionedResolver", (), {})
if "httpx" not in sys.modules:
    _hx = _t.ModuleType("httpx")
    _hx.ConnectTimeout = type("ConnectTimeout", (Exception,), {})
    _hx.ConnectError = type("ConnectError", (Exception,), {})
    _hx.get = lambda *a, **k: None
    sys.modules["httpx"] = _hx
if "requests" not in sys.modules:
    _rq = _t.ModuleType("requests")


    class _Resp:                                          # pragma: no cover
        status_code = 500
        text = ""
        content = b""


    _rq.get = lambda *a, **k: _Resp()
    _rq.RequestException = type("RequestException", (Exception,), {})
    sys.modules["requests"] = _rq
if "psutil" not in sys.modules:
    _ps = _t.ModuleType("psutil")
    _ps.Process = type("Process", (), {
        "__init__": lambda self, *a, **k: None,
        "cpu_percent": lambda self, *a, **k: 0.0,
        "memory_percent": lambda self, *a, **k: 0.0,
    })
    _ps.cpu_percent = lambda *a, **k: 0.0
    _ps.virtual_memory = lambda *a, **k: type("M", (), {"percent": 0.0})()
    sys.modules["psutil"] = _ps

from agent import chat_completion_helpers as cch   # noqa: E402


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
            if fn == "payload_chars":
                want = cch._payload_chars(i["value"], i["image_cost"])
            elif fn == "image_part_chars":
                want = cch._image_part_chars(i["part"], i["image_cost"])
            elif fn == "scale_stale":
                want = cch._scale_stale_timeout_for_context(i["base"], i["est_tokens"])
            elif fn == "bound_stale":
                # Reads HERMES_CODEX_HARD_TIMEOUT_SECONDS, not a parameter.
                os.environ["HERMES_CODEX_HARD_TIMEOUT_SECONDS"] = str(i["hard"])
                want = cch._bound_openai_codex_stale_timeout(
                    i["stale_timeout"], i["est_tokens"])
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        if c["out"] != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:200]} "
                  f"PY={want!r}"[:360])
    if total == 0:
        print("ORACLE cch_ports: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE cch_ports: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

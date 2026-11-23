#!/usr/bin/env python3
"""Differential oracle for gate w14_chatcomple_840065 (agent/chat_completion_local.py).

Loads the LIVE module via importlib.spec_from_file_location with stubbed
`utils.env_int`, hermes_cli.local_runtime.capabilities and
agent.chat_completion_helpers. The three seams inject env value, the managed
provider verdict, and the collaborator results used by retry_recovered_local_server.
"""
import importlib.util
import json
import os
import sys
import types
from pathlib import Path
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "chat_completion_local.py").is_file():
    QUARRY = Path("C:/Users/eman5/hermes-agent")
sys.path.insert(0, str(QUARRY))

_ENV_KEY = "HERMES_STREAM_STALE_GIVEUP"


def ref_env_int(key, default=0):
    raw = os.getenv(key, "").strip()
    try:
        return int(raw) if raw else default
    except (ValueError, TypeError):
        return default


_utils = types.ModuleType("utils")
_utils.env_int = ref_env_int
sys.modules["utils"] = _utils
_agent = types.ModuleType("agent")
_agent.__path__ = []
sys.modules["agent"] = _agent
_hc = types.ModuleType("hermes_cli")
_hc.__path__ = []
sys.modules["hermes_cli"] = _hc
_lr = types.ModuleType("hermes_cli.local_runtime")
_lr.__path__ = []
sys.modules["hermes_cli.local_runtime"] = _lr
_hc.local_runtime = _lr
_caps = types.ModuleType("hermes_cli.local_runtime.capabilities")
_caps.is_managed_provider = lambda provider, base_url: False
sys.modules["hermes_cli.local_runtime.capabilities"] = _caps
_lr.capabilities = _caps
_helpers = types.ModuleType("agent.chat_completion_helpers")
_helpers._stale_streak = lambda agent: 0
sys.modules["agent.chat_completion_helpers"] = _helpers
_agent.chat_completion_helpers = _helpers

_SRC = str(QUARRY / "agent" / "chat_completion_local.py")
_SPEC = importlib.util.spec_from_file_location("w14_chatcomple_live", _SRC)
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def expected(rec):
    f = rec.get("func")
    if f == "stale_giveup":
        if rec.get("env_value") is None:
            os.environ.pop(_ENV_KEY, None)
        else:
            os.environ[_ENV_KEY] = str(rec["env_value"])
        return mod.stale_giveup()
    if f == "streams_llamacpp":
        _caps.is_managed_provider = lambda provider, base_url: bool(rec.get("managed"))
        agent = SimpleNamespace(api_mode=rec.get("api_mode"), provider=rec.get("provider"),
                                base_url=rec.get("base_url"))
        return bool(mod._streams_llamacpp(agent))
    if f == "retry_recovered_local_server":
        mod.stale_giveup = lambda: int(rec.get("giveup") or 0)
        _helpers._stale_streak = lambda agent: int(rec.get("streak") or 0)
        mod._streams_llamacpp = lambda agent: bool(rec.get("streams"))
        mod._local_server_answers = lambda agent: bool(rec.get("answers"))
        agent = SimpleNamespace()
        ret = bool(mod.retry_recovered_local_server(agent))
        return {"result": ret, "set": getattr(agent, "_consecutive_stale_streams", None)}
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if got != want:
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w14_chatcomple_840065: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()

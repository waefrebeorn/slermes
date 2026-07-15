"""Oracle for gateway/platforms/_http_client_limits.py port.

Loads the LIVE upstream module and resolves platform_httpx_limits() under the
fixture's env overrides, emitting the same compact JSON the C harness does
(available / keepalive_expiry / max_keepalive) for byte-diffing.

argv[1] = fixture: {"env": {...}}.
"""
import sys, os, json
from pathlib import Path

ROOT = Path("/home/wubu/hermes-agent-dev")
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "gateway" / "platforms"))

import importlib.util
spec = importlib.util.spec_from_file_location(
    "gateway.platforms._http_client_limits",
    str(ROOT / "gateway" / "platforms" / "_http_client_limits.py"))
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
sys.modules["gateway.platforms._http_client_limits"] = mod
spec.loader.exec_module(mod)


def main():
    if len(sys.argv) < 2:
        return 1
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    for k, v in data.get("env", {}).items():
        os.environ[k] = v

    lim = mod.platform_httpx_limits()
    if lim is None:
        out = {"available": False, "keepalive_expiry": None, "max_keepalive": None}
    else:
        out = {
            "available": True,
            "keepalive_expiry": lim.keepalive_expiry,
            "max_keepalive": lim.max_keepalive_connections,
        }
    sys.stdout.write(json.dumps(out, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    sys.exit(main())

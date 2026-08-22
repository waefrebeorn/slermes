#!/usr/bin/env python3
"""sta_oracle_media_policy.py — Python twin for src/gateway/media_policy.c."""
import importlib.util
import json
import os
import sys

_LIVE = os.environ.get(
    "HERMES_LIVE_ROOT",
    os.path.join(os.path.expanduser("~"), ".hermes/hermes-agent"),
)
_spec = importlib.util.spec_from_file_location(
    "media_policy_live", os.path.join(_LIVE, "gateway", "media_policy.py")
)
_mp = importlib.util.module_from_spec(_spec)
sys.modules["media_policy_live"] = _mp
# Stub the hermes_cli.config lazy import so config=None never touches disk
import types
_fake_cfg = types.ModuleType("hermes_cli.config")
_fake_cfg.load_config = lambda: {}
sys.modules["hermes_cli.config"] = _fake_cfg
_spec.loader.exec_module(_mp)

ENVS = [
    "HERMES_MEDIA_DELIVERY_STRICT",
    "HERMES_MEDIA_ALLOW_DIRS",
    "HERMES_MEDIA_TRUST_RECENT_FILES",
]


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        for e in ENVS:
            os.environ.pop(e, None)
        for k, v in (c.get("preset") or {}).items():
            os.environ[k] = v
        cfg = c.get("config")
        _mp.apply_media_policy_env(cfg)
        out = {e: os.environ.get(e) for e in ENVS}
        print(json.dumps({"out": out}, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()

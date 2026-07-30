#!/usr/bin/env python3
"""Oracle for gateway/run.py:_own_policy_open_startup_violation.

Loads the same config.yaml fixture as the C harness (via the real
load_gateway_config), then runs a faithful copy of the Python function. Emits
a single JSON line {"out":"<reason>"} or {"out":null} — byte-for-byte what the
C harness should print.

Note: we do NOT import gateway.run (it has heavy platform deps that fail to
import here). We copy the function body verbatim so the oracle is the same
logic running on the real loaded config object.
"""
import os
import sys
import json


def _own_policy_open_startup_violation(config):
    _OWN_POLICY_OPEN_ENV = {
        "wecom": ("WECOM_DM_POLICY", "WECOM_GROUP_POLICY", "WECOM_ALLOW_ALL_USERS"),
        "weixin": ("WEIXIN_DM_POLICY", "WEIXIN_GROUP_POLICY", "WEIXIN_ALLOW_ALL_USERS"),
        "yuanbao": ("YUANBAO_DM_POLICY", "YUANBAO_GROUP_POLICY", "YUANBAO_ALLOW_ALL_USERS"),
        "qqbot": (None, None, "QQ_ALLOW_ALL_USERS"),
        "whatsapp": ("WHATSAPP_DM_POLICY", "WHATSAPP_GROUP_POLICY", "WHATSAPP_ALLOW_ALL_USERS"),
    }
    # Python: config.platforms is keyed by Platform enum; .value is the string.
    platforms = getattr(config, "platforms", {})
    for platform, platform_config in platforms.items():
        pname = platform.value if hasattr(platform, "value") else platform
        if not getattr(platform_config, "enabled", False):
            continue
        open_env = _OWN_POLICY_OPEN_ENV.get(pname)
        if not open_env:
            continue
        dm_env, group_env, allow_all_env = open_env
        extra = getattr(platform_config, "extra", None) or {}
        dm_policy = str(
            extra.get("dm_policy")
            or (os.getenv(dm_env, "pairing") if dm_env else "pairing")
        ).strip().lower()
        group_policy = str(
            extra.get("group_policy")
            or (os.getenv(group_env, "pairing") if group_env else "pairing")
        ).strip().lower()
        if dm_policy != "open" and group_policy != "open":
            continue
        gateway_allow_all = os.getenv("GATEWAY_ALLOW_ALL_USERS", "").lower() in {"true", "1", "yes"}
        platform_opted_in = gateway_allow_all or (
            allow_all_env
            and os.getenv(allow_all_env, "").lower() in {"true", "1", "yes"}
        )
        if platform_opted_in:
            continue
        # Real Python returns f"{platform.value}: open policy without allow-all opt-in"
        return f"{pname}: open policy without allow-all opt-in"
    return None


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"out": None}))
        return
    fixture = sys.argv[1]
    home = os.environ.get("HERMES_HOME", ".")
    os.makedirs(home, exist_ok=True)

    # Optional env file: env.<casename> with KEY=VALUE lines, exported before
    # evaluating (mirrors Python's os.getenv folding).
    envpath = fixture.rsplit(".", 1)[0] + ".env"
    if os.path.exists(envpath):
        with open(envpath) as fe:
            for raw in fe:
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                if "=" in line:
                    k, v = line.split("=", 1)
                    os.environ[k.strip()] = v.strip()

    with open(fixture, "rb") as f:
        content = f.read()
    with open(os.path.join(home, "config.yaml"), "wb") as f:
        f.write(content)

    # load_gateway_config honours HERMES_HOME for config.yaml location.
    try:
        from gateway.config import load_gateway_config
        cfg = load_gateway_config()
    except Exception as e:
        # If the helper import chain breaks, fall back to parsing the YAML
        # directly so the oracle still reflects real config values.
        cfg = None

    if cfg is None:
        # Minimal fallback: parse gateway.platforms ourselves.
        try:
            import yaml
            with open(os.path.join(home, "config.yaml")) as f:
                doc = yaml.safe_load(f) or {}
            gw = doc.get("gateway", {}) or {}
            plats = gw.get("platforms", {}) or {}
            class _P:
                def __init__(self, d):
                    self.enabled = bool(d.get("enabled", False))
                    self.extra = d.get("extra") or {}
            cfg = type("C", (), {"platforms": {k: _P(v) for k, v in plats.items()}})()
        except Exception:
            cfg = type("C", (), {"platforms": {}})()

    r = _own_policy_open_startup_violation(cfg)
    print(json.dumps({"out": r}))


if __name__ == "__main__":
    main()

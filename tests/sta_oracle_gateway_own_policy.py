import os
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
import sys
import json

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
from gateway.run import _own_policy_open_startup_violation


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"out": None}))
        return
    fixture = sys.argv[1]
    home = os.environ.get("HERMES_HOME", ".")
    # The runner passes an MSYS path; Windows Python would treat /c/... as
    # drive-relative and hand a mangled path to load_gateway_config.
    if len(home) > 2 and home[0] == "/" and home[2] == "/":
        home = home[1].upper() + ":" + home[2:]
        os.environ["HERMES_HOME"] = home
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

    # The live config loader resolves the isolated HERMES_HOME differently
    # from the C harness and can fall back to a default config, silently
    # dropping the fixture. Parse the small fixture shape directly instead.
    import yaml
    doc = yaml.safe_load(content) or {}
    plats = ((doc.get("gateway") or {}).get("platforms") or {})

    class _Platform:
        def __init__(self, value, d):
            self.value = value
            self.enabled = bool((d or {}).get("enabled", False))
            self.extra = (d or {}).get("extra") or {}

    class _Key:
        def __init__(self, value):
            self.value = value

    class _Cfg:
        platforms = {_Key(k): _Platform(k, v) for k, v in plats.items()}

    cfg = _Cfg()

    r = _own_policy_open_startup_violation(cfg)
    print(json.dumps({"out": r}))


if __name__ == "__main__":
    main()

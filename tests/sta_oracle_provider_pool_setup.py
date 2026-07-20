#!/usr/bin/env python3
"""
sta_oracle_provider_pool_setup.py — oracle for
t_port_provider_pool_setup.c.

Recomputes each provider name from the LIVE hermes_cli.auth.PROVIDER_REGISTRY
using the exact canonical logic of hermes_cli/setup.py:
_supports_same_provider_pool_setup():

    def _supports_same_provider_pool_setup(provider):
        if not provider or provider == "custom":
            return False
        if provider == "openrouter":
            return True
        pconfig = PROVIDER_REGISTRY.get(provider)
        if not pconfig:
            return False
        return pconfig.auth_type in {"api_key", "oauth_device_code"}

Emits one JSON line per input line, same shape as the C harness. The runner
diffs them, so any drift between the C registry snapshot and the live registry
is caught.
"""
import sys
import json
import importlib.util


def _load_auth():
    for base in sys.path:
        cand = f"{base}/hermes_cli/auth.py"
        try:
            spec = importlib.util.spec_from_file_location("live_auth", cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    import hermes_cli.auth as mod  # type: ignore
    return mod


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_provider_pool_setup.py <providers.txt>\n")
        return 2
    with open(sys.argv[1], "rb") as f:
        raw = f.read()
    text = raw.decode("utf-8")

    auth = _load_auth()
    REGISTRY = auth.PROVIDER_REGISTRY

    SUPPORTED = {"api_key", "oauth_device_code"}

    for line in text.split("\n"):
        name = line.strip()
        if not name:
            continue
        if not name or name == "custom":
            sup = False
        elif name == "openrouter":
            sup = True
        else:
            pconfig = REGISTRY.get(name)
            if not pconfig:
                sup = False
            else:
                sup = getattr(pconfig, "auth_type", None) in SUPPORTED
        sys.stdout.write(
            '{"provider":%s,"supports":%s}\n'
            % (json.dumps(name), "true" if sup else "false")
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())

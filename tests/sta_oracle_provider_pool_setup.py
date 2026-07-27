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
import os
import json
import importlib.util

# CRITICAL: as a script, sys.path[0] is tests/ — which contains hermes_cli/,
# providers/ and agent/ TEST packages that shadow the real ones. auth.py
# builds PROVIDER_REGISTRY partly from the providers catalog; with tests/
# shadowing `providers`, newer entries (novita, fireworks, solar, upstage,
# aliases) silently vanish and every `supports` flag flips to False.
_TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path[:] = [p for p in sys.path if os.path.abspath(p or ".") != _TESTS_DIR]


def _load_auth():
    # Resolve the LIVE hermes-agent tree first (same convention as the other
    # sta_oracle_* scripts): sibling of the slermes checkout, then
    # HERMES_AGENT_DIR, then the canonical ~/.hermes/hermes-agent install,
    # and only then fall back to whatever is importable on sys.path. The
    # sys.path scan alone silently loaded a STALE site-packages auth.py that
    # predates newer providers (novita/fireworks/solar/...), flipping their
    # supports flags to False while the C registry mirrored the live tree.
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, "..", ".."),                    # hermes-agent-dev
        os.environ.get("HERMES_AGENT_DIR", ""),
        os.path.expanduser("~/.hermes/hermes-agent"),
    ]
    # NOTE: the runner overrides HOME to a temp dir, so expanduser can miss —
    # also try the passwd-derived home explicitly.
    try:
        import pwd
        real_home = pwd.getpwuid(os.getuid()).pw_dir
        candidates.append(os.path.join(real_home, ".hermes", "hermes-agent"))
    except Exception:
        pass
    seen = set()
    for base in candidates + list(sys.path):
        if not base or base in seen:
            continue
        seen.add(base)
        cand = os.path.join(base, "hermes_cli", "auth.py")
        if not os.path.isfile(cand):
            continue
        try:
            spec = importlib.util.spec_from_file_location("live_auth", cand)
            mod = importlib.util.module_from_spec(spec)
            # MUST register before exec: dataclasses resolves string field
            # annotations via sys.modules[cls.__module__]; without this the
            # @dataclass decorator inside auth.py raises AttributeError and
            # the loader silently fell through to a stale registry.
            sys.modules["live_auth"] = mod
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            sys.modules.pop("live_auth", None)
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

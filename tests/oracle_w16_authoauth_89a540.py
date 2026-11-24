#!/usr/bin/env python3
"""Oracle for gate w16_authoauth_89a540 (hermes_cli/auth_oauth_grants.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

# ruamel stub: hermes_yaml is pulled transitively; these pure helpers never use it.
ruamel = types.ModuleType("ruamel"); ruamel.__path__ = []
ry = types.ModuleType("ruamel.yaml"); ry.__path__ = []
class YAML:
    def __init__(self, *a, **k): pass
    def load(self, *a, **k): return None
    def dump(self, *a, **k): pass
    def indent(self, *a, **k): return self
ry.YAML = YAML
err = types.ModuleType("ruamel.yaml.error")
class YAMLError(Exception): pass
err.YAMLError = YAMLError
res = types.ModuleType("ruamel.yaml.resolver")
class VersionedResolver: pass
res.VersionedResolver = VersionedResolver
ry.error = err; ry.resolver = res; ruamel.yaml = ry
sys.modules.update({"ruamel": ruamel, "ruamel.yaml": ry,
                    "ruamel.yaml.error": err, "ruamel.yaml.resolver": res})

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("hermes_cli.auth_oauth_grants", "hermes_cli/auth_oauth_grants.py")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    if func == "is_oauth_pool_payload":
        py = live._is_oauth_pool_payload(r.get("entry"))
    elif func == "is_forkable_pool_row":
        py = live._is_forkable_pool_row(r.get("provider_id"), r.get("entry"))
    elif func == "block_tokens":
        py = live._block_tokens(r.get("block"))
    elif func == "is_pkce_row":
        py = live._is_pkce_row(r.get("row"))
    elif func == "sign":
        py = live._sign(r.get("x"), r.get("y"))
    elif func == "constants":
        py = {
            "single_use": sorted(live.SINGLE_USE_REFRESH_POOL_PROVIDERS),
            "device_code": list(live._DEVICE_CODE_BLOCK_PROVIDERS),
            "token_fields": list(live._OAUTH_TOKEN_FIELDS),
            "singleton_files": list(live.SINGLE_USE_OAUTH_SINGLETON_FILES),
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w16_authoauth_89a540: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)

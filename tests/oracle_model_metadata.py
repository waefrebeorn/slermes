#!/usr/bin/env python3
"""Oracle for the pure slice of agent/model_metadata.py."""
import json
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# The module imports httpx at call time only, but its header pulls the provider
# registry; stub what is absent so the comparison stays about the pure helpers.
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
    _hx.TimeoutException = type("TimeoutException", (Exception,), {})
    _hx.get = lambda *a, **k: None
    sys.modules["httpx"] = _hx

from agent import model_metadata as mm   # noqa: E402


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
            if fn == "normalize_base_url":
                want = mm._normalize_base_url(i["base_url"])
            elif fn == "grok_effort":
                want = mm.grok_supports_reasoning_effort(i["model"])
            elif fn == "openai_rejects":
                want = mm.openai_model_rejects_reasoning(i["model"])
            elif fn == "grok_46":
                want = mm.is_grok_46_family(i["model"])
            elif fn == "fast_mode":
                want = mm.is_anthropic_fast_mode_model(i.get("model"))
            elif fn == "catalog_key":
                want = mm._catalog_key_matches(i["key"], i["model_lower"])
            elif fn == "longest_key":
                hit = mm._longest_key_match(i["table"], i["model_lower"])
                want = None if hit is None else [hit[0], hit[1]]
            elif fn == "local_endpoint":
                want = mm.is_local_endpoint(i["base_url"])
            elif fn == "localhost_ipv4":
                want = mm._localhost_to_ipv4(i["url"])
            elif fn == "server_root":
                want = mm._server_root(i["base_url"])
            elif fn == "lmstudio_root":
                want = mm._lmstudio_server_root(i["base_url"])
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
        print("ORACLE model_metadata: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE model_metadata: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())

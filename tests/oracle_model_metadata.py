#!/usr/bin/env python3
"""Oracle for the pure slice of agent/model_metadata.py."""
import json
import os
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

try:
    from agent import model_metadata as mm   # noqa: E402
    # Call-time deps of the batch-2 ports: _strip_openrouter_routing_variant
    # infers the provider via the registry, and _is_codex_route imports the
    # config helpers even when the custom-provider list is passed explicitly.
    # Pre-import them so a missing dep triggers the venv re-exec below rather
    # than a false mismatch at comparison time.
    import providers                        # noqa: F401,E402
    from hermes_cli import config as _hcfg  # noqa: F401,E402
except ImportError:
    _venv = os.path.expanduser("~/hermes-agent/.venv/Scripts/python.exe")
    if not os.path.isfile(_venv):
        _venv = os.path.expanduser("~/hermes-agent/.venv/bin/python")
    if os.path.isfile(_venv) and \
            os.path.abspath(sys.executable) != os.path.abspath(_venv):
        os.execv(_venv, [_venv, os.path.abspath(__file__)] + sys.argv[1:])
    raise


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
            elif fn == "strip_or_variant":
                want = mm._strip_openrouter_routing_variant(
                    i["model"], i["base_url"], i["provider"])
            elif fn == "codex_route":
                want = mm._is_codex_route(
                    i["provider"], i["base_url"], i["custom_providers"])
            elif fn == "flat_ctx":
                want = mm._extract_flat_context_length(i["payload"])
            elif fn == "model_payload_ctx":
                want = mm._context_length_from_model_payload(i["payload"])
            elif fn == "token_rates":
                want = mm._normalize_token_rates(dict(i["pricing"]),
                                                 i.get("unit"))
            elif fn == "split_budget":
                want = mm._completion_split_budget(i["error_lower"])
            elif fn == "sglang":
                want = mm._sglang_window_and_input(i["error_lower"])
                if want is not None:
                    want = list(want)
            elif fn == "phrase_group":
                want = mm._any_phrase_group(
                    i["text"], tuple(tuple(g) for g in i["groups"]))
            elif fn == "suggests_minimax":
                want = mm._model_name_suggests_minimax(i["model"])
            elif fn == "stale_32k":
                want = mm._model_name_suggests_stale_32k_underreport(i["model"])
            elif fn == "positive_int":
                want = mm._positive_int(i["value"])
            elif fn == "bare_codex_slug":
                want = mm._bare_codex_slug(i["model"])
            elif fn == "codex_variant_base":
                want = mm._codex_variant_base(i["model"])
            elif fn == "bedrock_ctx":
                want = mm._is_bedrock_context(i["base_url"], i["provider"])
            elif fn == "codex_catalog_key":
                want = mm._codex_catalog_key(i["keys"], i["slug"])
            elif fn == "parse_base_url":
                p = mm._parse_base_url(i["base_url"], i.get("scheme") or "http")
                if p is None:
                    want = None
                else:
                    try:
                        port, perr = p.port, False
                    except ValueError:
                        port, perr = None, True
                    want = {"scheme": p.scheme, "netloc": p.netloc,
                            "path": p.path, "params": p.params,
                            "query": p.query, "fragment": p.fragment,
                            "hostname": p.hostname, "port": port,
                            "port_invalid": perr, "username": p.username,
                            "password": p.password}
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

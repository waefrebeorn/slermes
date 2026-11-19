#!/usr/bin/env python3
"""Generator for tests/fixtures/model_metadata_flow.jsonl.

Emits >=5 cases per ported function of agent/model_metadata.py (the UNCLAIMED
remainder ported as src/cli/model_metadata_flow.c). Each case is
{"func": <name>, "in": {...args..., "io": {seam: script}, "env": {...},
"state": {...}}}.

Script shapes (shared with the C universal seam mm_call and the oracle):
  - absent key            -> None
  - {"$seq": [...]}       -> per-call queue (exhaustion raises RuntimeError)
  - {"__raise__": cls, "msg": m} -> raises a REAL exception of that class
  - any other JSON value  -> reused for every call

Domain restrictions (documented in the port header):
  - str()/float()/int() operands are ASCII strings, numbers, bools or null;
    int() tokens allow underscores BETWEEN digits only; paths/URLs POSIX ASCII.
  - memo/cache keys are opaque strings (production uses tuples; key-agnostic).
  - isinstance(int) positions carry integral ints or bools (no integral floats).
  - file content is scripted at the json.load level; BOM/decode errors ride as
    open/json.load raises.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "model_metadata_flow.jsonl")

CASES = []


def case(func, **inp):
    CASES.append({"func": func, "in": inp})


def io(**scripts):
    return scripts


def seq(*vals):
    return {"$seq": list(vals)}


def raise_(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


# ── _count_parts ────────────────────────────────────────────────────────────
case("_count_parts", parts=[{"type": "image"}, {"type": "text"}], types=["image"])
case("_count_parts", parts=[{"type": "image"}, {"type": "image_url"}], types=["image", "image_url"])
case("_count_parts", parts=[{"type": "text"}], types=["image"])
case("_count_parts", parts="notlist", types=["image"])
case("_count_parts", parts=[{"type": "image"}, "notdict", {"no_type": 1}], types=["image"])
case("_count_parts", parts=[], types=["image"])
case("_count_parts", parts=[{"type": None}], types=["image"])

# ── _lmstudio_loaded_context ────────────────────────────────────────────────
case("_lmstudio_loaded_context", model={"loaded_instances": [{"config": {"context_length": 4096}}]})
case("_lmstudio_loaded_context", model={"loaded_instances": [{"config": {"context_length": 0}}, {"config": {"context_length": 8192}}]})
case("_lmstudio_loaded_context", model={"loaded_instances": []})
case("_lmstudio_loaded_context", model={})
case("_lmstudio_loaded_context", model={"loaded_instances": [{"config": {"context_length": -5}}]})
case("_lmstudio_loaded_context", model={"loaded_instances": ["notdict", {"config": "notdict"}, {"config": {"context_length": 2048}}]})
case("_lmstudio_loaded_context", model={"loaded_instances": [{"config": {"context_length": True}}]})
case("_lmstudio_loaded_context", model={"loaded_instances": None})

# ── _codex_catalog_urls ─────────────────────────────────────────────────────
case("_codex_catalog_urls", base_url="")
case("_codex_catalog_urls", base_url="https://gw.example.com")
case("_codex_catalog_urls", base_url="https://gw.example.com/")
case("_codex_catalog_urls", base_url="  https://gw.example.com  ")
case("_codex_catalog_urls", base_url=None)
case("_codex_catalog_urls", base_url="https://gw.example.com/v1")

# ── _ollama_show_context ────────────────────────────────────────────────────
case("_ollama_show_context", data={"parameters": "num_ctx 4096"}, gguf_first=False)
case("_ollama_show_context", data={"model_info": {"llama.context_length": 8192}}, gguf_first=True)
case("_ollama_show_context", data={"parameters": "num_ctx 4096", "model_info": {"x.context_length": 8192}}, gguf_first=False)
case("_ollama_show_context", data={"parameters": "num_ctx 4096", "model_info": {"x.context_length": 8192}}, gguf_first=True)
case("_ollama_show_context", data={"parameters": "num_ctx abc"}, gguf_first=False)
case("_ollama_show_context", data={"parameters": "num_ctx 100", "model_info": {"x.context_length": 8192}}, gguf_first=False, minimum=1024)
case("_ollama_show_context", data={"parameters": "num_ctx 2048"}, gguf_first=False, minimum=1024)
case("_ollama_show_context", data={"parameters": "num_ctx 2048"}, gguf_first=False, minimum=4096)
case("_ollama_show_context", data={}, gguf_first=False)
case("_ollama_show_context", data={"model_info": {"x.context_length": True}}, gguf_first=True)
case("_ollama_show_context", data={"parameters": "num_ctx 1_024"}, gguf_first=False)
case("_ollama_show_context", data={"parameters": None}, gguf_first=False)
case("_ollama_show_context", data={"model_info": None}, gguf_first=True)
case("_ollama_show_context", data=None, gguf_first=False)
case("_ollama_show_context", data={"parameters": "num_ctx 4096\nnum_ctx 8192"}, gguf_first=False)

# ── _endpoint_model_entry ───────────────────────────────────────────────────
case("_endpoint_model_entry", model={"name": "Model"}, model_id="m1", context_length=4096,
     io=io(extract_first_int=8192, extract_pricing={"prompt": "0.001"}))
case("_endpoint_model_entry", model={}, model_id="m1", context_length=None,
     io=io(extract_first_int=None, extract_pricing={}))
case("_endpoint_model_entry", model={"name": None}, model_id="m1", context_length=1024,
     io=io(extract_first_int=None, extract_pricing=None))
case("_endpoint_model_entry", model={"name": "X"}, model_id="m", context_length=None,
     io=io(extract_first_int=None, extract_pricing={"prompt": "0"}))
case("_endpoint_model_entry", model={"name": "X"}, model_id="m", context_length=None,
     io=io(extract_first_int=seq()))
case("_endpoint_model_entry", model={"name": "X"}, model_id="m", context_length=None,
     io=io(extract_first_int=raise_("ValueError", "bad")))
case("_endpoint_model_entry", model={"name": "X"}, model_id="m", context_length=None,
     io=io(extract_first_int=raise_("SystemExit", "1")))
case("_endpoint_model_entry", model="notdict", model_id="m", context_length=None,
     io=io(extract_first_int=None, extract_pricing={}))

# ── _note_if_connect_timeout ────────────────────────────────────────────────
case("_note_if_connect_timeout", exc={"__raise__": "ConnectTimeout", "msg": "t"}, base_url="http://h",
     io=io(is_connect_timeout=True, note_endpoint_blackholed=None))
case("_note_if_connect_timeout", exc={"__raise__": "ConnectTimeout", "msg": "t"}, base_url="http://h",
     io=io(is_connect_timeout=False))
case("_note_if_connect_timeout", exc=None, base_url="http://h", io=io(is_connect_timeout=False))
case("_note_if_connect_timeout", exc={"__raise__": "ConnectTimeout", "msg": "t"}, base_url="http://h",
     io=io(is_connect_timeout=raise_("RuntimeError", "x")))
case("_note_if_connect_timeout", exc={"__raise__": "ConnectTimeout", "msg": "t"}, base_url="http://h",
     io=io(is_connect_timeout=True, note_endpoint_blackholed=raise_("SystemExit", "1")))
case("_note_if_connect_timeout", exc={"__raise__": "ConnectTimeout", "msg": "t"}, base_url="http://h",
     io=io(is_connect_timeout=raise_("KeyboardInterrupt", "")))

# ── _save_unless_skipped ────────────────────────────────────────────────────
case("_save_unless_skipped", model="m", base_url="http://h", ctx=4096, provider="custom",
     io=io(skip_persistent_context_cache=False, save_context_length=None))
case("_save_unless_skipped", model="m", base_url="http://h", ctx=4096, provider="lmstudio",
     io=io(skip_persistent_context_cache=True))
case("_save_unless_skipped", model="m", base_url="http://h", ctx=4096, provider="custom",
     io=io(skip_persistent_context_cache=raise_("RuntimeError", "x")))
case("_save_unless_skipped", model="m", base_url="http://h", ctx=4096, provider="custom",
     io=io(skip_persistent_context_cache=False, save_context_length=raise_("SystemExit", "1")))
case("_save_unless_skipped", model="m", base_url="http://h", ctx=4096, provider="custom", io=io())
case("_save_unless_skipped", model="m", base_url="http://h", ctx=4096, provider="openai-codex",
     io=io(skip_persistent_context_cache=True))

# ── _probe_local_context_length ─────────────────────────────────────────────
case("_probe_local_context_length", model="m", base_url="http://h", api_key="k", provider="custom",
     io=io(query_local_context_length=4096, skip_persistent_context_cache=False, maybe_cache_local_context_length=None))
case("_probe_local_context_length", model="m", base_url="http://h", api_key="k", provider="custom",
     io=io(query_local_context_length=None))
case("_probe_local_context_length", model="m", base_url="http://h", api_key="k", provider="custom",
     io=io(query_local_context_length=0))
case("_probe_local_context_length", model="m", base_url="http://h", api_key="k", provider="custom",
     io=io(query_local_context_length=-5))
case("_probe_local_context_length", model="m", base_url="http://h", api_key="k", provider="lmstudio",
     io=io(query_local_context_length=4096, skip_persistent_context_cache=True))
case("_probe_local_context_length", model="m", base_url="http://h", api_key="k", provider="custom",
     io=io(query_local_context_length=raise_("RuntimeError", "x")))
case("_probe_local_context_length", model="m", base_url="http://h", api_key="k", provider="custom",
     io=io(query_local_context_length=4096, skip_persistent_context_cache=False, maybe_cache_local_context_length=raise_("SystemExit", "1")))
case("_probe_local_context_length", model="m", base_url="http://h", api_key="k", provider="custom",
     io=io(query_local_context_length="str"))

# ── save_provider_context_length ────────────────────────────────────────────
case("save_provider_context_length", model="m", base_url="http://h", length=4096, provider="custom",
     io=io(is_bedrock_context=False, save_context_length=None))
case("save_provider_context_length", model="m", base_url="", length=4096, provider="bedrock",
     io=io(is_bedrock_context=True, save_context_length=None))
case("save_provider_context_length", model="m", base_url="https://bedrock-runtime.us-east-1.amazonaws.com", length=4096, provider="bedrock",
     io=io(is_bedrock_context=True, save_context_length=None))
case("save_provider_context_length", model="m", base_url="http://h", length=4096, provider="custom",
     io=io(is_bedrock_context=raise_("RuntimeError", "x")))
case("save_provider_context_length", model="m", base_url="http://h", length=4096, provider="custom",
     io=io(is_bedrock_context=False, save_context_length=raise_("SystemExit", "1")))
case("save_provider_context_length", model="m", base_url=None, length=4096, provider="custom",
     io=io(is_bedrock_context=False, save_context_length=None))

# ── _is_ollama_server ───────────────────────────────────────────────────────
case("_is_ollama_server", base_url="http://h", api_key="k", io=io(detect_local_server_type="ollama"))
case("_is_ollama_server", base_url="http://h", api_key="k", io=io(detect_local_server_type="lm-studio"))
case("_is_ollama_server", base_url="http://h", api_key="k", io=io(detect_local_server_type=None))
case("_is_ollama_server", base_url="http://h", api_key="k", io=io(detect_local_server_type=raise_("RuntimeError", "x")))
case("_is_ollama_server", base_url="http://h", api_key="k", io=io(detect_local_server_type=raise_("SystemExit", "1")))
case("_is_ollama_server", base_url="http://h", api_key="k", io=io(detect_local_server_type=raise_("KeyboardInterrupt", "")))
case("_is_ollama_server", base_url="http://h", api_key="k", io=io())

# ── _cache_file ─────────────────────────────────────────────────────────────
case("_cache_file", name="x.json", io=io(get_hermes_home="/h"))
case("_cache_file", name="x.json", io=io(get_hermes_home="/h/"))
case("_cache_file", name="sub/x.json", io=io(get_hermes_home="/home/user"))
case("_cache_file", name="x.json", io=io(get_hermes_home="/"))
case("_cache_file", name="x.json", io=io(get_hermes_home=raise_("RuntimeError", "x")))
case("_cache_file", name="x.json", io=io(get_hermes_home=None))
case("_cache_file", name="x.json", io=io(get_hermes_home=raise_("SystemExit", "1")))

# ── _load_json_dict ─────────────────────────────────────────────────────────
case("_load_json_dict", path="/c/x.json", io=io(open=True, **{"json.load": {"a": 1}}))
case("_load_json_dict", path="/c/x.json", io=io(open=True, **{"json.load": [1, 2]}))
case("_load_json_dict", path="/c/x.json", io=io(open=True, **{"json.load": None}))
case("_load_json_dict", path="/c/x.json", io=io(open=raise_("FileNotFoundError", "no")))
case("_load_json_dict", path="/c/x.json", io=io(open=True, **{"json.load": raise_("JSONDecodeError", "bad")}))
case("_load_json_dict", path="/c/x.json", io=io(open=raise_("SystemExit", "1")))
case("_load_json_dict", path="/c/x.json", io=io(open=True, **{"json.load": raise_("KeyboardInterrupt", "")}))
case("_load_json_dict", path="/c/x.json", io=io())

# ── _ttl_memo_get ───────────────────────────────────────────────────────────
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {"k": {"ts": 100, "value": 42}}}, **{"time.time": 200}))
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {"k": {"ts": 100, "value": 42}}}, **{"time.time": 500}))
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {"k": "notdict"}}, **{"time.time": 200}))
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {"k": {"value": 42}}}, **{"time.time": 200}))
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {"k": {"ts": "bad", "value": 42}}}, **{"time.time": 200}))
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {"k": {"ts": 100}}}, **{"time.time": 200}))
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {"k": {"ts": 100, "value": None}}}, **{"time.time": 200}))
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {}}, **{"time.time": 200}))
case("_ttl_memo_get", path="/c/m.json", key="k", ttl=300, ts_key="ts", value_key="value",
     io=io(open=True, **{"json.load": {"k": {"ts": 100, "value": 42}}}, **{"time.time": raise_("SystemExit", "1")}))

# ── _ttl_memo_put ───────────────────────────────────────────────────────────
case("_ttl_memo_put", path="/c/m.json", key="k", value=42, ttl=300, ts_key="ts", value_key="value", what="memo", ts_first=False,
     io=io(**{"time.time": 200}, open=True, **{"json.load": {}}, atomic_json_write=None))
case("_ttl_memo_put", path="/c/m.json", key="k", value=42, ttl=300, ts_key="ts", value_key="value", what="memo", ts_first=True,
     io=io(**{"time.time": 200}, open=True, **{"json.load": {}}, atomic_json_write=None))
case("_ttl_memo_put", path="/c/m.json", key="k", value=42, ttl=300, ts_key="ts", value_key="value", what="memo", ts_first=False,
     io=io(**{"time.time": 200}, open=True, **{"json.load": {"old": {"ts": 100, "value": 1}, "expired": {"ts": -200, "value": 2}, "notdict": 5}}, atomic_json_write=None))
case("_ttl_memo_put", path="/c/m.json", key="k", value=42, ttl=300, ts_key="ts", value_key="value", what="memo", ts_first=False,
     io=io(**{"time.time": 200}, open=True, **{"json.load": {"bad": {"ts": None}}}, atomic_json_write=None))
case("_ttl_memo_put", path="/c/m.json", key="k", value=42, ttl=300, ts_key="ts", value_key="value", what="memo", ts_first=False,
     io=io(**{"time.time": raise_("RuntimeError", "clock")}))
case("_ttl_memo_put", path="/c/m.json", key="k", value=42, ttl=300, ts_key="ts", value_key="value", what="memo", ts_first=False,
     io=io(**{"time.time": 200}, open=True, **{"json.load": {}}, atomic_json_write=raise_("OSError", "disk")))
case("_ttl_memo_put", path="/c/m.json", key="k", value=42, ttl=300, ts_key="ts", value_key="value", what="memo", ts_first=False,
     io=io(**{"time.time": raise_("SystemExit", "1")}))
case("_ttl_memo_put", path="/c/m.json", key="k", value=42, ttl=300, ts_key="ts", value_key="value", what="memo", ts_first=False,
     io=io(**{"time.time": 200}, open=True, **{"json.load": {"bad": {"ts": "xyz"}}}, atomic_json_write=None))

# ── _memo_local_probe ───────────────────────────────────────────────────────
case("_memo_local_probe", cache_key="k", state={}, io=io(**{"time.monotonic": 100}, probe=4096))
case("_memo_local_probe", cache_key="k", state={"_LOCAL_CTX_PROBE_CACHE": {"k": [4096, 90]}}, io=io(**{"time.monotonic": 100}))
case("_memo_local_probe", cache_key="k", state={"_LOCAL_CTX_PROBE_CACHE": {"k": [4096, 0]}}, io=io(**{"time.monotonic": 100}, probe=8192))
case("_memo_local_probe", cache_key="k", state={}, io=io(**{"time.monotonic": 100}, probe=None))
case("_memo_local_probe", cache_key="k", state={}, io=io(**{"time.monotonic": 100}, probe=0))
case("_memo_local_probe", cache_key="k", state={}, io=io(**{"time.monotonic": 100}, probe=raise_("RuntimeError", "x")))
case("_memo_local_probe", cache_key="k", state={}, io=io(**{"time.monotonic": raise_("SystemExit", "1")}))

# ── _bedrock_probe_failed_recently ──────────────────────────────────────────
case("_bedrock_probe_failed_recently", key="k", state={}, io=io(**{"time.monotonic": 100}))
case("_bedrock_probe_failed_recently", key="k", state={"_BEDROCK_PROBE_FAILURE_CACHE": {"k": 90}}, io=io(**{"time.monotonic": 100}))
case("_bedrock_probe_failed_recently", key="k", state={"_BEDROCK_PROBE_FAILURE_CACHE": {"k": 0}}, io=io(**{"time.monotonic": 400}))
case("_bedrock_probe_failed_recently", key="k", state={"_BEDROCK_PROBE_FAILURE_CACHE": {"k": 200, "other": 0}}, io=io(**{"time.monotonic": 400}))
case("_bedrock_probe_failed_recently", key="k", state={"_BEDROCK_PROBE_FAILURE_CACHE": {"k": 90}}, io=io(**{"time.monotonic": raise_("SystemExit", "1")}))
case("_bedrock_probe_failed_recently", key="absent", state={"_BEDROCK_PROBE_FAILURE_CACHE": {"k": 90}}, io=io(**{"time.monotonic": 100}))

# ── _remember_no_codex_catalog ──────────────────────────────────────────────
case("_remember_no_codex_catalog", cache_key="k", state={}, io=io(**{"time.time": 1000}, bound_codex_caches=None))
case("_remember_no_codex_catalog", cache_key="k", state={"_codex_oauth_context_cache": {"k": [{"a": 1}, 900]}}, io=io(**{"time.time": 1000}))
case("_remember_no_codex_catalog", cache_key="k", state={"_codex_oauth_context_cache": {"k": [{}, 900]}}, io=io(**{"time.time": 1000}, bound_codex_caches=None))
case("_remember_no_codex_catalog", cache_key="k", state={"_codex_oauth_context_cache": {"k": [{"a": 1}, 0]}}, io=io(**{"time.time": 5000}, bound_codex_caches=None))
case("_remember_no_codex_catalog", cache_key="k", state={}, io=io(**{"time.time": raise_("SystemExit", "1")}))
case("_remember_no_codex_catalog", cache_key="k", state={}, io=io(**{"time.time": 1000}, bound_codex_caches=raise_("RuntimeError", "x")))

# ── _cached_codex_catalog_max ───────────────────────────────────────────────
case("_cached_codex_catalog_max", access_token="tok", base_url="", slug="gpt-5",
     state={"_codex_oauth_max_context_cache": {"fp1": {"gpt-5": 900000}}},
     io=io(codex_oauth_token_fingerprint="fp1", codex_catalog_key="gpt-5"))
case("_cached_codex_catalog_max", access_token="tok", base_url="", slug="gpt-5",
     state={"_codex_oauth_max_context_cache": {"fp1": {"gpt-5": 900000}}},
     io=io(codex_oauth_token_fingerprint="fp1", codex_catalog_key=None))
case("_cached_codex_catalog_max", access_token="tok", base_url="", slug="gpt-5", state={},
     io=io(codex_oauth_token_fingerprint="fp1", codex_catalog_key="gpt-5"))
case("_cached_codex_catalog_max", access_token="tok", base_url="", slug="gpt-5", state={},
     io=io(codex_oauth_token_fingerprint=raise_("RuntimeError", "x")))
case("_cached_codex_catalog_max", access_token="tok", base_url="", slug="gpt-5", state={},
     io=io(codex_oauth_token_fingerprint="fp1", codex_catalog_key=raise_("SystemExit", "1")))
case("_cached_codex_catalog_max", access_token="tok", base_url="", slug="gpt-5",
     state={"_codex_oauth_max_context_cache": {"fp1": {"gpt-5": 900000}}},
     io=io(codex_oauth_token_fingerprint="fp2", codex_catalog_key="gpt-5"))

# ── _validate_cached_context_length ─────────────────────────────────────────
case("_validate_cached_context_length", model="m", base_url="http://h", cached=0, api_key="",
     io=io(model_name_suggests_stale_32k_underreport=False, stale_pre_catalog_cache_entry=False, invalidate_cached_context_length=None))
case("_validate_cached_context_length", model="kimi-k2", base_url="http://h", cached=32768, api_key="",
     io=io(model_name_suggests_stale_32k_underreport=True, stale_pre_catalog_cache_entry=False, invalidate_cached_context_length=None))
case("_validate_cached_context_length", model="m", base_url="http://h", cached=100000, api_key="",
     io=io(stale_pre_catalog_cache_entry=True, invalidate_cached_context_length=None))
case("_validate_cached_context_length", model="m", base_url="https://inference-api.nousresearch.com/v1", cached=100000, api_key="",
     io=io(stale_pre_catalog_cache_entry=False, infer_provider_from_url="nous"))
case("_validate_cached_context_length", model="m", base_url="http://localhost:11434", cached=100000, api_key="k",
     io=io(stale_pre_catalog_cache_entry=False, infer_provider_from_url=None, is_local_endpoint=True, reconcile_local_cached_context_length=200000))
case("_validate_cached_context_length", model="m", base_url="https://api.openai.com/v1", cached=100000, api_key="",
     io=io(stale_pre_catalog_cache_entry=False, infer_provider_from_url="openai", is_local_endpoint=False))
case("_validate_cached_context_length", model="m", base_url="http://h", cached=0, api_key="",
     io=io(model_name_suggests_stale_32k_underreport=raise_("RuntimeError", "x")))
case("_validate_cached_context_length", model="m", base_url="http://h", cached=100000, api_key="",
     io=io(stale_pre_catalog_cache_entry=raise_("SystemExit", "1")))

# ── fetch_codex_catalog_entries ─────────────────────────────────────────────
case("fetch_codex_catalog_entries", base_url="",
     io=io(get=seq({"status_code": 200, "json": {"models": [{"slug": "a"}]}}, {"status_code": 200, "json": {"models": []}})))
case("fetch_codex_catalog_entries", base_url="",
     io=io(get=seq({"status_code": 404, "json": {}}, {"status_code": 200, "json": {"models": [{"slug": "b"}]}})))
case("fetch_codex_catalog_entries", base_url="",
     io=io(get=seq({"status_code": 200, "json": {"models": []}}, {"status_code": 200, "json": {"models": []}})))
case("fetch_codex_catalog_entries", base_url="",
     io=io(get=seq({"status_code": 500, "json": {}}, {"status_code": 403, "json": {}})))
case("fetch_codex_catalog_entries", base_url="",
     io=io(get=seq({"status_code": 200, "json": "notdict"}, {"status_code": 200, "json": {"models": [{"slug": "c"}]}})))
case("fetch_codex_catalog_entries", base_url="", io=io(get=raise_("RuntimeError", "net")))
case("fetch_codex_catalog_entries", base_url="", io=io(get=raise_("SystemExit", "1")))

# ── _codex_catalog_probe_allowed ────────────────────────────────────────────
case("_codex_catalog_probe_allowed", access_token="tok", base_url="https://gw.example.com", io=io(base_url_host_matches=False))
case("_codex_catalog_probe_allowed", access_token="tok", base_url="https://chatgpt.com", io=io(base_url_host_matches=True, decode_jwt_claims={"sub": "x"}))
case("_codex_catalog_probe_allowed", access_token="tok", base_url="https://chatgpt.com", io=io(base_url_host_matches=True, decode_jwt_claims=None))
case("_codex_catalog_probe_allowed", access_token="", base_url="https://chatgpt.com", io=io())
case("_codex_catalog_probe_allowed", access_token=None, base_url="https://chatgpt.com", io=io())
case("_codex_catalog_probe_allowed", access_token="tok", base_url="", io=io(base_url_host_matches=True, decode_jwt_claims={"sub": "x"}))
case("_codex_catalog_probe_allowed", access_token="tok", base_url="https://chatgpt.com", io=io(base_url_host_matches=raise_("SystemExit", "1")))
case("_codex_catalog_probe_allowed", access_token="tok", base_url="https://gw.example.com", io=io(base_url_host_matches=raise_("RuntimeError", "x")))

# ── _parse_models_payload ───────────────────────────────────────────────────
case("_parse_models_payload", payload={"data": [{"id": "m1", "name": "Model"}]},
     io=io(extract_first_int=seq(4096, 8192), extract_pricing={}, add_model_aliases=None))
case("_parse_models_payload", payload={"data": [{"id": "org/m1", "name": "M"}]},
     io=io(extract_first_int=seq(None, None), extract_pricing={}, add_model_aliases=None))
case("_parse_models_payload", payload={"data": []}, io=io())
case("_parse_models_payload", payload={"data": ["notdict", {"id": ""}, {"id": "m2"}]},
     io=io(extract_first_int=seq(1024, 2048), extract_pricing={}, add_model_aliases=None))
case("_parse_models_payload", payload="notdict", io=io())
case("_parse_models_payload", payload={"data": [{"id": "m1"}]}, io=io(extract_first_int=raise_("RuntimeError", "x")))
case("_parse_models_payload", payload={"data": [{"id": "m1"}]},
     io=io(extract_first_int=seq(4096, 8192), extract_pricing={}, add_model_aliases=raise_("SystemExit", "1")))

# ── estimate_native_anthropic_messages_tokens_rough ─────────────────────────
case("estimate_native_anthropic_messages_tokens_rough", messages=[{"role": "user", "content": "hi"}],
     io=io(native_anthropic_accounting_projection=[[{"role": "user", "content": "hi"}], [100, 200]], estimate_messages_tokens_rough=50))
case("estimate_native_anthropic_messages_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=[[], []], estimate_messages_tokens_rough=0))
case("estimate_native_anthropic_messages_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=[[], [10]], estimate_messages_tokens_rough=5))
case("estimate_native_anthropic_messages_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=raise_("RuntimeError", "x")))
case("estimate_native_anthropic_messages_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=[[], []], estimate_messages_tokens_rough=raise_("SystemExit", "1")))
case("estimate_native_anthropic_messages_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=[[]]))
case("estimate_native_anthropic_messages_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=None))

# ── estimate_native_anthropic_request_tokens_rough ──────────────────────────
case("estimate_native_anthropic_request_tokens_rough", messages=[{"role": "user", "content": "hi"}], system_prompt="sys", tools=[{"type": "function"}],
     io=io(native_anthropic_accounting_projection=[[], [100]], estimate_request_tokens_rough=200))
case("estimate_native_anthropic_request_tokens_rough", messages=[], system_prompt="", tools=None,
     io=io(native_anthropic_accounting_projection=[[], []], estimate_request_tokens_rough=10))
case("estimate_native_anthropic_request_tokens_rough", messages=[], system_prompt="", tools=None,
     io=io(native_anthropic_accounting_projection=[[], [5, 5]], estimate_request_tokens_rough=20))
case("estimate_native_anthropic_request_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=raise_("SystemExit", "1")))
case("estimate_native_anthropic_request_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=[[], []], estimate_request_tokens_rough=raise_("RuntimeError", "x")))
case("estimate_native_anthropic_request_tokens_rough", messages=[],
     io=io(native_anthropic_accounting_projection=[[], [], []]))


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    # per-function coverage check (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        raise SystemExit(f"generator: functions with <5 cases: {short}")
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"gen_model_metadata_flow_cases: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")


if __name__ == "__main__":
    main()
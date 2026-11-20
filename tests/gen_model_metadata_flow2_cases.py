#!/usr/bin/env python3
"""Deterministic case generator for model_metadata_flow2 (7 cases/function).

Each case is {"func": name, "in": {args..., "io": {seam: script}}}. The harness
replays it through the C port and the oracle replays the SAME scripts through
the LIVE quarry def; their {ret,trace,state} must agree. Run:

    python tests/gen_model_metadata_flow2_cases.py
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "model_metadata_flow2.jsonl"


def c(func, inp, io=None):
    rec = dict(inp)
    rec["io"] = io or {}
    return {"func": func, "in": rec}


def seq(*items):
    return {"$seq": list(items)}


def raises(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


CASES = []

# ── _ollama_show: client seam, exception hierarchy, blackhole note ────────
CASES += [
    c("_ollama_show",
      {"server_url": "http://host:11434", "api_key": "k1", "bare_model": "m1"},
      {"auth_headers": {"Authorization": "Bearer k1"}, "resolve_verify": True,
       "httpx_client": "client",
       "post": {"status_code": 200, "json": {"context_length": 32768, "details": {"family": "llama"}}}}),
    c("_ollama_show",
      {"server_url": "http://host:11434", "api_key": "k1", "bare_model": "m1"},
      {"auth_headers": {}, "resolve_verify": False, "httpx_client": "client",
       "post": {"status_code": 404, "json": {"error": "not found"}}}),
    c("_ollama_show",
      {"server_url": "http://host:11434", "api_key": "k1", "bare_model": "m1",
       "note_blackhole": True},
      {"auth_headers": raises("ConnectTimeout", "connect timed out")}),
    c("_ollama_show",
      {"server_url": "http://host:11434", "api_key": "k1", "bare_model": "m1"},
      {"auth_headers": {}, "resolve_verify": True, "httpx_client": "client",
       "post": raises("ValueError", "bad json")}),
    c("_ollama_show",
      {"server_url": "http://host:11434", "api_key": "k1", "bare_model": "m1"},
      {"auth_headers": {}, "resolve_verify": True, "httpx_client": "client",
       "post": raises("SystemExit", "bye")}),
    c("_ollama_show",
      {"server_url": "http://host:11434", "bare_model": "m2", "timeout": 7.5,
       "note_blackhole": True},
      {"auth_headers": None, "resolve_verify": True, "httpx_client": "client",
       "post": {"status_code": 200, "json": {"ok": True, "nested": [1, 2, 3]}}}),
    c("_ollama_show",
      {"server_url": "http://host:11434", "api_key": "k1", "bare_model": "m1",
       "note_blackhole": True},
      {"auth_headers": {}, "resolve_verify": raises("OSError", "verify failed")}),
]

# ── _lmstudio_context: matching, short-circuit, lazy positive_int ─────────
CASES += [
    c("_lmstudio_context", {"lmstudio_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 503, "json": {}}}),
    c("_lmstudio_context", {"lmstudio_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"models": []}}}),
    c("_lmstudio_context", {"lmstudio_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"models": [
          {"key": "m1", "loaded_instances": [{"config": {"context_length": 32768}}]}]}},
       "model_id_matches": True, "positive_int": 32768}),
    c("_lmstudio_context", {"lmstudio_url": "http://h:1234", "model": "right"},
      {"get": {"status_code": 200, "json": {"models": [
          {"key": "wrong", "id": "right", "loaded_instances": [
              {"config": {"context_length": 65536}}]}]}},
       "model_id_matches": seq(False, True), "positive_int": 65536}),
    c("_lmstudio_context", {"lmstudio_url": "http://h:1234", "model": "second"},
      {"get": {"status_code": 200, "json": {"models": [
          {"key": "first"},
          {"key": "second", "loaded_instances": [
              {"config": {"context_length": 1000}},
              {"config": {"context_length": 131072}}]}]}},
       "model_id_matches": seq(False, False, True),
       "positive_int": seq(None, 131072)}),
    c("_lmstudio_context", {"lmstudio_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"models": [
          {"key": "m1", "loaded_instances": []}]}},
       "model_id_matches": True}),
    c("_lmstudio_context", {"lmstudio_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"models": [{"key": "m1"}]}},
       "model_id_matches": raises("TypeError", "bad id")}),
]

# ── _openai_models_list_context: id match, single-model fallback, readers ─
CASES += [
    c("_openai_models_list_context", {"server_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 404, "json": {}}}),
    c("_openai_models_list_context", {"server_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"data": []}}}),
    c("_openai_models_list_context", {"server_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"data": [{"id": "m1", "n_ctx": 8192}]}},
       "model_id_matches": True, "positive_int": 8192,
       "context_length_from_model_payload": 16384}),
    c("_openai_models_list_context", {"server_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"data": [{"id": "m1", "max_tokens": 16384}]}},
       "model_id_matches": True, "positive_int": None,
       "context_length_from_model_payload": 16384}),
    c("_openai_models_list_context", {"server_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"data": [{"id": "m1", "meta": {"n_ctx": 9999}}]}},
       "model_id_matches": True, "positive_int": seq(None, 131072),
       "context_length_from_model_payload": 200000}),
    c("_openai_models_list_context", {"server_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"data": ["path/to/model.gguf"]}}}),
    c("_openai_models_list_context", {"server_url": "http://h:1234", "model": "m1"},
      {"get": {"status_code": 200, "json": {"data": [{"id": "m1"}]}},
       "model_id_matches": True, "positive_int": raises("ZeroDivisionError", "div0")}),
]

# ── _load_context_cache_document: file seams + exception path ─────────────
CASES += [
    c("_load_context_cache_document", {},
      {"get_context_cache_path": "/home/u/.hermes/context_length_cache.yaml",
       "exists": False}),
    c("_load_context_cache_document", {},
      {"get_context_cache_path": "/home/u/.hermes/context_length_cache.yaml",
       "exists": True, "open": "f", "yaml.safe_load": [1, 2, 3]}),
    c("_load_context_cache_document", {},
      {"get_context_cache_path": "/home/u/.hermes/context_length_cache.yaml",
       "exists": True, "open": "f", "yaml.safe_load": {
           "context_lengths": {"m1": 4096}, "bedrock_confirmed_v1": {"m2": True}, "other": 1}}),
    c("_load_context_cache_document", {},
      {"get_context_cache_path": "/home/u/.hermes/context_length_cache.yaml",
       "exists": True, "open": "f", "yaml.safe_load": {"other": 1}}),
    c("_load_context_cache_document", {},
      {"get_context_cache_path": "/home/u/.hermes/context_length_cache.yaml",
       "exists": True, "open": raises("FileNotFoundError", "no file")}),
    c("_load_context_cache_document", {},
      {"get_context_cache_path": "/home/u/.hermes/context_length_cache.yaml",
       "exists": True, "open": "f", "yaml.safe_load": raises("UnicodeError", "bad utf8")}),
    c("_load_context_cache_document", {},
      {"get_context_cache_path": raises("SystemExit", "no home")}),
]

# ── _config_override_context_length: two suppress(Exception) blocks ───────
CASES += [
    c("_config_override_context_length", {}),
    c("_config_override_context_length",
      {"provider": "openai", "model": "gpt-4o"},
      {"override_context_window": 131072}),
    c("_config_override_context_length",
      {"provider": "openai", "model": "gpt-4o", "base_url": "http://x"},
      {"override_context_window": 0, "get_custom_provider_context_length": 8192}),
    c("_config_override_context_length",
      {"provider": "openai", "model": "gpt-4o", "base_url": "http://x",
       "custom_providers": [{"id": "p1"}]},
      {"override_context_window": raises("ImportError", "no models_dev"),
       "get_custom_provider_context_length": 4096}),
    c("_config_override_context_length",
      {"provider": "openai", "model": "gpt-4o", "base_url": "http://x"},
      {"override_context_window": raises("ValueError", "v"),
       "get_custom_provider_context_length": raises("KeyError", "k")}),
    c("_config_override_context_length",
      {"provider": "openai", "model": "gpt-4o"},
      {"override_context_window": raises("SystemExit", "halt")}),
    c("_config_override_context_length",
      {"provider": "openai", "model": "gpt-4o", "base_url": "http://x"},
      {"override_context_window": None, "get_custom_provider_context_length": None}),
]

# ── _lmstudio_native_models: composed seam flow + cache aliasing ──────────
CASES += [
    c("_lmstudio_native_models", {"normalized": "http://h:1234", "headers": {"k": "v"}},
      {"lmstudio_server_root": "http://h:1234", "resolve_verify": True,
       "http_get": {"status_code": 200, "json": {"models": []}},
       "raise_for_status": None}),
    c("_lmstudio_native_models", {"normalized": "http://h:1234", "headers": {"k": "v"}},
      {"lmstudio_server_root": "http://h:1234", "resolve_verify": True,
       "http_get": {"status_code": 200, "json": {"models": [
           {"key": "m1", "name": "Model One", "loaded_instances": [
               {"config": {"context_length": 4096}}]}]}},
       "raise_for_status": None, "lmstudio_loaded_context": 4096,
       "endpoint_model_entry": {"name": "Model One", "context_length": 4096},
       "add_model_aliases": None}),
    c("_lmstudio_native_models", {"normalized": "http://h:1234/api/v1", "headers": {"k": "v"}},
      {"lmstudio_server_root": "http://h:1234", "resolve_verify": False,
       "http_get": {"status_code": 200, "json": {"models": [
           {"key": "pub/m1", "id": "alias", "loaded_instances": [
               {"config": {"context_length": 8192}}]}]}},
       "raise_for_status": None, "lmstudio_loaded_context": 8192,
       "endpoint_model_entry": {"name": "Pub", "context_length": 8192},
       "add_model_aliases": seq(None, None)}),
    c("_lmstudio_native_models", {"normalized": "http://h:1234", "headers": None},
      {"lmstudio_server_root": "http://h:1234", "resolve_verify": True,
       "http_get": {"status_code": 200, "json": {"models": [
           {"id": "only-id", "loaded_instances": []}]}},
       "raise_for_status": None, "lmstudio_loaded_context": None,
       "endpoint_model_entry": {"name": "only-id"}, "add_model_aliases": None}),
    c("_lmstudio_native_models", {"normalized": "http://h:1234", "headers": {}},
      {"lmstudio_server_root": "http://h:1234", "resolve_verify": True,
       "http_get": {"status_code": 200, "json": {"models": [
           "not-a-dict", {}, {"key": "m2"}]}},
       "raise_for_status": None, "lmstudio_loaded_context": None,
       "endpoint_model_entry": {"name": "m2"}, "add_model_aliases": None}),
    c("_lmstudio_native_models", {"normalized": "http://h:1234", "headers": {"k": "v"}},
      {"lmstudio_server_root": "http://h:1234", "resolve_verify": True,
       "http_get": {"status_code": 200, "json": {"models": []}},
       "raise_for_status": raises("RuntimeError", "http error")}),
    c("_lmstudio_native_models", {"normalized": "http://h:1234", "headers": {"k": "v"}},
      {"lmstudio_server_root": "http://h:1234", "resolve_verify": True,
       "http_get": {"status_code": 200, "json": {"models": [{"key": "m1"}]}},
       "raise_for_status": None, "lmstudio_loaded_context": 4096,
       "endpoint_model_entry": raises("ValueError", "bad entry")}),
]

assert len(CASES) == 42, len(CASES)
OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for rec in CASES:
        f.write(json.dumps(rec, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT}")

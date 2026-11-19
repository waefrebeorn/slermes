#!/usr/bin/env python3
"""Generate tests/fixtures/web_tools_flow.jsonl for the tools/web_tools.py FLOW
port (src/tools/web_tools_flow.c).

Case shape: {"func": <name>, "in": {...args..., "io": {seam: script}}}.
Scripts (shared contract with the C universal seam wt_call and the oracle):
  - MISSING key            -> fail-closed RuntimeError("no script for <name>")
  - null                   -> None result
  - {"$seq": [v1, v2, ...]}-> one result per call; exhaustion raises
                              RuntimeError("script exhausted: <name>")
  - {"__raise__": cls, "msg": m, ...} -> raises; trace ret is the WHOLE marker
  - any other value        -> reused on every call (a bare ARRAY is a LIST
                              RESULT, not a queue — wrap queues in $seq)

Seam names are the LIVE collaborator names. Claimed siblings seamed:
  _load_web_config, _has_env, _ddgs_package_importable,
  _list_registered_web_providers, _registered_web_provider,
  _ensure_web_plugins_loaded, _get_extract_char_limit, _rescue_eligible,
  _policy_blocked_result, read_selection, selection_exists, selection_error.
Provider methods seam as provider.<method>; search_memo methods as
search_memo.<method>; _debug methods as debug_log_call / debug_save.

Fixture domain (documented in the port header): ASCII strings; plain base-10
numeric strings for int()/float(); str(exc) consumers use scripted raises whose
class has str==msg (no KeyError / None.search receiver shapes); _registry_call
func_name in {get_provider, list_providers}; _probe method in {is_available,
is_keyless_available}; _merge_in_order fixed keys are stringified ints.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "web_tools_flow.jsonl"


def seq(*vals):
    return {"$seq": list(vals)}


def rais(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def case(func, **inp):
    return {"func": func, "in": inp}


CASES = []


def add(*cases):
    CASES.extend(cases)


# Autodetect probe scripts: all _has_env false unless a $seq overrides.
def auto_io(has_env=False, gw=False, ddgs=False, providers=None, **extra):
    io = {"_has_env": has_env, "_is_tool_gateway_ready": gw,
          "_ddgs_package_importable": ddgs,
          "_list_registered_web_providers": providers if providers is not None else []}
    io.update(extra)
    return io


# ── _configured_backend ─────────────────────────────────────────────────────
add(
    case("_configured_backend", key="backend", io={"_load_web_config": {"backend": " Tavily "}}),
    case("_configured_backend", key="backend", io={"_load_web_config": {"backend": "NOUS"}}),
    case("_configured_backend", key="backend", io={"_load_web_config": {}}),
    case("_configured_backend", key="backend", io={"_load_web_config": {"backend": None}}),
    case("_configured_backend", key="backend", io={"_load_web_config": {"backend": ""}}),
    case("_configured_backend", key="search_backend", io={"_load_web_config": {"search_backend": "Exa"}}),
    case("_configured_backend", key="extract_backend", io={"_load_web_config": {"backend": "x"}}),
    case("_configured_backend", key="backend", io={"_load_web_config": {"backend": 5}}),
    case("_configured_backend", key="backend", io={"_load_web_config": rais("RuntimeError")}),
    case("_configured_backend", key="backend", io={}),
)

# ── _registry_call ──────────────────────────────────────────────────────────
add(
    case("_registry_call", func_name="get_provider", default=None, args=["firecrawl"],
         io={"get_provider": {"name": "firecrawl"}}),
    case("_registry_call", func_name="get_provider", default=None, args=["x"],
         io={"get_provider": None}),
    case("_registry_call", func_name="list_providers", default=[], args=[],
         io={"list_providers": [{"name": "a"}]}),
    case("_registry_call", func_name="list_providers", default=[], args=[],
         io={"list_providers": rais("RuntimeError")}),
    case("_registry_call", func_name="get_provider", default="DEF", args=["x"],
         io={"get_provider": rais("ValueError")}),
    case("_registry_call", func_name="get_provider", default=None, args=["x"],
         io={"get_provider": rais("SystemExit")}),
    case("_registry_call", func_name="list_providers", default=[], args=[], io={}),
    case("_registry_call", func_name="get_provider", default=None, args=["x"],
         io={"get_provider": rais("KeyboardInterrupt")}),
)

# ── _probe ──────────────────────────────────────────────────────────────────
add(
    case("_probe", provider={"name": "x"}, method="is_available", io={"provider.is_available": True}),
    case("_probe", provider={"name": "x"}, method="is_available", io={"provider.is_available": False}),
    case("_probe", provider={"name": "x"}, method="is_available", io={"provider.is_available": None}),
    case("_probe", provider={"name": "x"}, method="is_keyless_available", io={"provider.is_keyless_available": 1}),
    case("_probe", provider={"name": "x"}, method="is_available", io={"provider.is_available": rais("ValueError")}),
    case("_probe", provider={"name": "x"}, method="is_available", io={"provider.is_available": rais("SystemExit")}),
    case("_probe", provider=None, method="is_available", io={}),
    case("_probe", provider="str", method="is_available", io={}),
    case("_probe", provider={"name": "x"}, method="is_available", io={}),
    case("_probe", provider={"name": "x"}, method="is_available", io={"provider.is_available": "yes"}),
)

# ── _autodetect_backend ─────────────────────────────────────────────────────
add(
    case("_autodetect_backend", io=auto_io(has_env=seq(True, False, False, False, False, False, False, False, False))),
    case("_autodetect_backend", io=auto_io(has_env=False, gw=True)),
    case("_autodetect_backend", io=auto_io(has_env=False, ddgs=True)),
    case("_autodetect_backend", io=auto_io(has_env=False, providers=[{"name": "custom"}],
                                           **{"provider.is_available": True})),
    case("_autodetect_backend", io=auto_io(has_env=False, providers=[{"name": "exa"}],
                                           **{"provider.is_available": True})),
    case("_autodetect_backend", io=auto_io(has_env=False, providers=[{"name": "custom"}],
                                           **{"provider.is_available": False})),
    case("_autodetect_backend", io=auto_io(has_env=False)),
    case("_autodetect_backend", io=auto_io(has_env=seq(False, False, False, False, False, True, False, False, False))),
    case("_autodetect_backend", io=auto_io(has_env=rais("RuntimeError"))),
    case("_autodetect_backend", io=auto_io(has_env=False, gw=rais("ValueError"))),
    case("_autodetect_backend", io={"_has_env": False}),
)

# ── _keyless_backend ────────────────────────────────────────────────────────
add(
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": None, "_keyless_tier_enabled": True,
                                 "_keyless_preference": ["exa"], "_registered_web_provider": {"name": "exa"},
                                 "provider.is_keyless_available": True}),
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": None, "_keyless_tier_enabled": False}),
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": None, "_keyless_tier_enabled": True,
                                 "_keyless_preference": []}),
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": None, "_keyless_tier_enabled": True,
                                 "_keyless_preference": ["exa"], "_registered_web_provider": None}),
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": None, "_keyless_tier_enabled": True,
                                 "_keyless_preference": ["exa", "parallel"],
                                 "_registered_web_provider": seq(None, {"name": "parallel"}),
                                 "provider.is_keyless_available": True}),
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": rais("RuntimeError")}),
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": None, "_keyless_tier_enabled": rais("ValueError")}),
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": rais("SystemExit")}),
    case("_keyless_backend", io={"_ensure_web_plugins_loaded": None, "_keyless_tier_enabled": True,
                                 "_keyless_preference": ["exa"], "_registered_web_provider": {"name": "exa"},
                                 "provider.is_keyless_available": False}),
    case("_keyless_backend", io={}),
)

# ── _managed_web_search ─────────────────────────────────────────────────────
add(
    case("_managed_web_search", io={"_load_web_config": {"search_backend": "nous"}}),
    case("_managed_web_search", io={"_load_web_config": {"search_backend": "tavily"}}),
    case("_managed_web_search", io={"_load_web_config": {}, "read_selection": "nous"}),
    case("_managed_web_search", io={"_load_web_config": {}, "read_selection": "exa"}),
    case("_managed_web_search", io={"_load_web_config": {}, "read_selection": None,
                                    "_has_env": False, "_is_tool_gateway_ready": True,
                                    "_ddgs_package_importable": False, "_list_registered_web_providers": []}),
    case("_managed_web_search", io={"_load_web_config": {}, "read_selection": None,
                                    "_has_env": seq(False, False, False, False, False, True, False, False, False),
                                    "_is_tool_gateway_ready": False, "_ddgs_package_importable": False,
                                    "_list_registered_web_providers": []}),
    case("_managed_web_search", io={"_load_web_config": {}, "read_selection": None,
                                    "_has_env": False, "_is_tool_gateway_ready": False,
                                    "_ddgs_package_importable": False, "_list_registered_web_providers": [],
                                    "resolve_free_search_gateway": {"ok": True}}),
    case("_managed_web_search", io={"_load_web_config": {}, "read_selection": None,
                                    "_has_env": False, "_is_tool_gateway_ready": False,
                                    "_ddgs_package_importable": False, "_list_registered_web_providers": [],
                                    "resolve_free_search_gateway": None}),
    case("_managed_web_search", io={"_load_web_config": rais("RuntimeError")}),
    case("_managed_web_search", io={"_load_web_config": {}, "read_selection": rais("ValueError")}),
)

# ── _xai_available ──────────────────────────────────────────────────────────
add(
    case("_xai_available", io={"has_xai_credentials": True}),
    case("_xai_available", io={"has_xai_credentials": False}),
    case("_xai_available", io={"has_xai_credentials": None}),
    case("_xai_available", io={"has_xai_credentials": rais("RuntimeError")}),
    case("_xai_available", io={"has_xai_credentials": rais("SystemExit")}),
    case("_xai_available", io={}),
)

# ── _finish_debug ───────────────────────────────────────────────────────────
add(
    case("_finish_debug", call_name="web_search_tool", debug_call_data={"error": None, "x": 1},
         error_msg="EM", io={"debug_log_call": None, "debug_save": None, "tool_error": "{\"success\": false}"}),
    case("_finish_debug", call_name="web_search_tool", debug_call_data={"error": None},
         error_msg=None, io={"debug_log_call": None, "debug_save": None}),
    case("_finish_debug", call_name="web_extract_tool", debug_call_data={"error": "pre"},
         error_msg="EM2", io={"debug_log_call": None, "debug_save": None, "tool_error": "ERR"}),
    case("_finish_debug", call_name="c", debug_call_data={}, error_msg="EM",
         io={"debug_log_call": rais("RuntimeError"), "debug_save": None, "tool_error": "ERR"}),
    case("_finish_debug", call_name="c", debug_call_data={}, error_msg=None,
         io={"debug_log_call": None, "debug_save": rais("ValueError")}),
    case("_finish_debug", call_name="c", debug_call_data={"a": 1}, error_msg="EM",
         io={"debug_log_call": None, "debug_save": None, "tool_error": rais("SystemExit")}),
    case("_finish_debug", call_name="c", debug_call_data={"a": 1}, error_msg="EM", io={}),
)

# ── _memoized_search ────────────────────────────────────────────────────────
MS_PROV = {"name": "tavily"}
add(
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": {"success": True, "data": {"web": [1, 2, 3]}},
             "slice_search_response": {"success": True, "data": {"web": [1, 2, 3]}}}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": seq(None, None), "search_memo.flight_lock": None,
             "bucket_limit": 10, "provider.search": {"success": True, "data": {"web": []}},
             "search_memo.store": None, "slice_search_response": {"success": True, "data": {"web": []}}}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": seq(None, {"success": True}), "search_memo.flight_lock": None,
             "slice_search_response": {"success": True}}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": seq(None, None), "search_memo.flight_lock": None,
             "bucket_limit": 10, "provider.search": rais("ValueError"),
             "_rescue_eligible": True, "search_with_failover": {"success": True, "data": {"web": []}},
             "slice_search_response": {"success": True, "data": {"web": []}}}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": seq(None, None), "search_memo.flight_lock": None,
             "bucket_limit": 10, "provider.search": rais("ValueError"),
             "_rescue_eligible": False, "slice_search_response": {"success": False}}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": seq(None, None), "search_memo.flight_lock": None,
             "bucket_limit": 10, "provider.search": {"success": False, "error": "SE"},
             "_rescue_eligible": True, "search_with_failover": {"success": True, "data": {"web": []}},
             "slice_search_response": {"success": True}}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": seq(None, None), "search_memo.flight_lock": None,
             "bucket_limit": 10, "provider.search": rais("SystemExit")}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": rais("RuntimeError")}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5,
         io={"search_memo.lookup": seq(None, None), "search_memo.flight_lock": rais("ValueError")}),
    case("_memoized_search", provider=MS_PROV, query="q", limit=5, io={}),
)

# ── _ring_vendor_keyless ────────────────────────────────────────────────────
add(
    case("_ring_vendor_keyless", name="firecrawl", io={"_use_keyless_ring": True}),
    case("_ring_vendor_keyless", name="firecrawl", io={"_use_keyless_ring": False}),
    case("_ring_vendor_keyless", name="exa", io={"get_provider_env": "KEY", "use_keyless": False}),
    case("_ring_vendor_keyless", name="exa", io={"get_provider_env": "", "use_keyless": True}),
    case("_ring_vendor_keyless", name="zzz", io={"use_keyless": True}),
    case("_ring_vendor_keyless", name="parallel", io={"get_provider_env": rais("RuntimeError"), "use_keyless": True}),
    case("_ring_vendor_keyless", name="firecrawl", io={"_use_keyless_ring": rais("SystemExit")}),
    case("_ring_vendor_keyless", name="keenable", io={"get_provider_env": "K", "use_keyless": None}),
)

# ── _managed_search_fallback ────────────────────────────────────────────────
MSF_IO = {"get_provider_env": "", "_load_web_config": {"search_backend": "nous"},
          "_is_tool_gateway_ready": True, "get_provider": {"name": "firecrawl"},
          "provider.search": {"success": True, "data": {"web": []}}}
add(
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="OE", query="q", limit=5, io=dict(MSF_IO)),
    case("_managed_search_fallback", provider={"name": "tavily"}, original_error="OE", query="q", limit=5, io={}),
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="OE", query="q", limit=5,
         io={"get_provider_env": "KEY"}),
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="OE", query="q", limit=5,
         io={"get_provider_env": "", "_load_web_config": {"search_backend": "tavily"}}),
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="OE", query="q", limit=5,
         io={"get_provider_env": "", "_load_web_config": {"search_backend": "nous"}, "_is_tool_gateway_ready": False}),
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="", query="q", limit=5,
         io={**MSF_IO, "provider.search": {"success": False, "error": "FE"}}),
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="OE", query="q", limit=5,
         io={**MSF_IO, "provider.search": rais("RuntimeError")}),
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="OE", query="q", limit=5,
         io={**MSF_IO, "get_provider": rais("ValueError")}),
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="OE", query="q", limit=5,
         io={**MSF_IO, "provider.search": rais("SystemExit")}),
    case("_managed_search_fallback", provider={"name": "perplexity"}, original_error="OE", query="q", limit=5,
         io={"get_provider_env": "", "_load_web_config": {"search_backend": "nous"},
             "_is_tool_gateway_ready": True, "get_provider": {"name": "firecrawl"},
             "provider.search": {"success": True}}),
)

# ── _rescue_search ──────────────────────────────────────────────────────────
add(
    case("_rescue_search", provider_name="tavily", original_error="OE", query="q", limit=5,
         io={"search_with_failover": {"success": True, "data": {"web": []}}}),
    case("_rescue_search", provider_name="tavily", original_error="OE", query="q", limit=5,
         io={"search_with_failover": {"success": False, "error": "RE"}}),
    case("_rescue_search", provider_name="tavily", original_error="", query="q", limit=5,
         io={"search_with_failover": {"success": False}}),
    case("_rescue_search", provider_name="exa", original_error="OE", query="q", limit=5,
         io={"search_with_failover": {"success": True}}),
    case("_rescue_search", provider_name="tavily", original_error="OE", query="q", limit=5,
         io={"search_with_failover": rais("RuntimeError")}),
    case("_rescue_search", provider_name="tavily", original_error="OE", query="q", limit=5,
         io={"search_with_failover": rais("SystemExit")}),
    case("_rescue_search", provider_name="tavily", original_error="OE", query="q", limit=5, io={}),
)

# ── _rescue_extract ─────────────────────────────────────────────────────────
RE_URLS = ["u1", "u2"]
RE_RES = [{"url": "u1", "error": "e1"}, {"url": "u2", "error": "e2"}]
add(
    case("_rescue_extract", provider_name="tavily", urls=RE_URLS, results=RE_RES,
         io={"_policy_blocked_result": seq(False, False),
             "extract_with_failover": [{"url": "u1", "content": "c1"}, {"url": "u2", "content": "c2"}]}),
    case("_rescue_extract", provider_name="tavily", urls=RE_URLS, results=RE_RES,
         io={"_policy_blocked_result": seq(True, True)}),
    case("_rescue_extract", provider_name="tavily", urls=RE_URLS, results=RE_RES,
         io={"_policy_blocked_result": seq(False, False),
             "extract_with_failover": [{"url": "u1", "error": "x"}, {"url": "u2", "error": "y"}]}),
    case("_rescue_extract", provider_name="tavily", urls=RE_URLS, results=RE_RES,
         io={"_policy_blocked_result": seq(True, False),
             "extract_with_failover": [{"url": "u2", "content": "c2"}]}),
    case("_rescue_extract", provider_name="tavily", urls=["u1", "u2", "u3"], results=RE_RES,
         io={"extract_with_failover": [{"url": "u1", "content": "c1"}]}),
    case("_rescue_extract", provider_name="tavily", urls=[], results=[], io={}),
    case("_rescue_extract", provider_name="tavily", urls=RE_URLS, results=RE_RES,
         io={"_policy_blocked_result": seq(False, False), "extract_with_failover": rais("RuntimeError")}),
    case("_rescue_extract", provider_name="tavily", urls=RE_URLS, results=RE_RES,
         io={"_policy_blocked_result": seq(False, False),
             "extract_with_failover": [{"url": "u1", "content": "c1"}, {"url": "u2", "content": "c2"}, {"url": "u3"}]}),
    case("_rescue_extract", provider_name="tavily", urls=RE_URLS,
         results=[{"url": "u1", "error": ""}, {"url": "u2", "error": "e2"}],
         io={"_policy_blocked_result": seq(False, False),
             "extract_with_failover": [{"url": "u1", "content": "c1"}, {"url": "u2", "content": "c2"}]}),
)

# ── _clamp_char_limit ───────────────────────────────────────────────────────
add(
    case("_clamp_char_limit", value=5000),
    case("_clamp_char_limit", value=100),
    case("_clamp_char_limit", value=999999),
    case("_clamp_char_limit", value="12345"),
    case("_clamp_char_limit", value=12.9),
    case("_clamp_char_limit", value=True),
    case("_clamp_char_limit", value="abc"),
    case("_clamp_char_limit", value=None),
    case("_clamp_char_limit", value=[1]),
    case("_clamp_char_limit", value="12.5"),
    case("_clamp_char_limit", value=""),
)

# ── _clamp_or_default ───────────────────────────────────────────────────────
add(
    case("_clamp_or_default", value=5000),
    case("_clamp_or_default", value=None),
    case("_clamp_or_default", value="abc"),
    case("_clamp_or_default", value=100),
    case("_clamp_or_default", value=[1]),
    case("_clamp_or_default", value="  6000  "),
    case("_clamp_or_default", value=False),
)

# ── _effective_char_limit ───────────────────────────────────────────────────
add(
    case("_effective_char_limit", char_limit=3000, io={}),
    case("_effective_char_limit", char_limit=None, io={"_get_extract_char_limit": 20000}),
    case("_effective_char_limit", char_limit="abc", io={}),
    case("_effective_char_limit", char_limit=100, io={}),
    case("_effective_char_limit", char_limit=None, io={"_get_extract_char_limit": rais("RuntimeError")}),
    case("_effective_char_limit", char_limit=None, io={}),
)

# ── _trim_results ───────────────────────────────────────────────────────────
add(
    case("_trim_results", results=[{"url": "u", "title": "t", "content": "c", "error": None, "extra": 1}]),
    case("_trim_results", results=[{"url": "u", "blocked_by_policy": {"host": "h"}}]),
    case("_trim_results", results=[]),
    case("_trim_results", results=[{}]),
    case("_trim_results", results=[{"url": "u", "error": "e"}, {"title": "t"}]),
    case("_trim_results", results=[{"url": "u", "content": "c", "blocked_by_policy": False}]),
)

# ── _disabled_plugin_error ──────────────────────────────────────────────────
add(
    case("_disabled_plugin_error", capability="search", disabled_key="plugins/web/tavily"),
    case("_disabled_plugin_error", capability="extract", disabled_key="plugins/web/firecrawl"),
    case("_disabled_plugin_error", capability="search", disabled_key="tavily"),
    case("_disabled_plugin_error", capability="search", disabled_key=""),
    case("_disabled_plugin_error", capability="extract", disabled_key="a/b/c"),
)

# ── _no_provider_error ──────────────────────────────────────────────────────
add(
    case("_no_provider_error", capability="search", fallback="FB", io={"_disabled_web_plugin_for": None}),
    case("_no_provider_error", capability="search", fallback="FB", io={"_disabled_web_plugin_for": "plugins/web/tavily"}),
    case("_no_provider_error", capability="extract", fallback="FB", io={"_disabled_web_plugin_for": ""}),
    case("_no_provider_error", capability="search", fallback="FB", io={"_disabled_web_plugin_for": rais("RuntimeError")}),
    case("_no_provider_error", capability="search", fallback="FB", io={}),
    case("_no_provider_error", capability="search", fallback="FB", io={"_disabled_web_plugin_for": rais("SystemExit")}),
)

# ── _strict_selection_error ─────────────────────────────────────────────────
add(
    case("_strict_selection_error", capability="extract", backend="foo",
         io={"selection_error": "SELERR", "_disabled_web_plugin_for": None}),
    case("_strict_selection_error", capability="search", backend="bar",
         io={"selection_error": "SELERR", "_disabled_web_plugin_for": "plugins/web/bar"}),
    case("_strict_selection_error", capability="extract", backend="foo",
         io={"selection_error": rais("RuntimeError"), "_disabled_web_plugin_for": None}),
    case("_strict_selection_error", capability="extract", backend="foo",
         io={"selection_error": "SELERR", "_disabled_web_plugin_for": rais("ValueError")}),
    case("_strict_selection_error", capability="extract", backend="",
         io={"selection_error": "SELERR", "_disabled_web_plugin_for": None}),
    case("_strict_selection_error", capability="extract", backend="foo", io={"_disabled_web_plugin_for": None}),
)

# ── _result_entry ───────────────────────────────────────────────────────────
add(
    case("_result_entry", url="u", error="e"),
    case("_result_entry", url="u", error=None),
    case("_result_entry", url="", error=""),
    case("_result_entry", url="u", error=False),
    case("_result_entry", url="u", error={"k": 1}),
)

# ── _extract_error_json ─────────────────────────────────────────────────────
add(
    case("_extract_error_json", error="boom"),
    case("_extract_error_json", error=""),
    case("_extract_error_json", error="quote\" and \\ backslash"),
    case("_extract_error_json", error="line\nbreak"),
    case("_extract_error_json", error="tab\there"),
)

# ── _refuse_all ─────────────────────────────────────────────────────────────
add(
    case("_refuse_all", error="boom"),
    case("_refuse_all", error=""),
    case("_refuse_all", error="quote\" x"),
    case("_refuse_all", error="newline\nx"),
    case("_refuse_all", error="Blocked: URL contains what appears to be an API key or token. Secrets must not be sent in URLs."),
)

# ── _merge_in_order ─────────────────────────────────────────────────────────
add(
    case("_merge_in_order", total=3, fixed={"0": {"url": "a"}}, fetch_positions=[1, 2],
         fetch_urls=["b", "c"], results=[{"url": "b"}]),
    case("_merge_in_order", total=2, fixed={}, fetch_positions=[0, 1],
         fetch_urls=["a", "b"], results=[{"url": "a"}, {"url": "b"}]),
    case("_merge_in_order", total=1, fixed={"0": {"url": "a"}}, fetch_positions=[],
         fetch_urls=[], results=[]),
    case("_merge_in_order", total=3, fixed={"1": {"url": "b"}}, fetch_positions=[0, 2],
         fetch_urls=["a", "c"], results=[]),
    case("_merge_in_order", total=2, fixed={}, fetch_positions=[0, 1],
         fetch_urls=["a", "b"], results=[{"url": "a"}, {"url": "b"}, {"url": "extra"}]),
    case("_merge_in_order", total=0, fixed={}, fetch_positions=[], fetch_urls=[], results=[]),
)

# ── _resolve_extract_provider ───────────────────────────────────────────────
add(
    case("_resolve_extract_provider", backend="firecrawl",
         io={"get_provider": {"name": "firecrawl", "display_name": "Firecrawl"}, "provider.supports_extract": True}),
    case("_resolve_extract_provider", backend="tavily",
         io={"get_provider": {"name": "tavily", "display_name": "Tavily"}, "provider.supports_extract": False}),
    case("_resolve_extract_provider", backend="foo",
         io={"get_provider": None, "selection_exists": True, "selection_error": "SELERR", "_disabled_web_plugin_for": None}),
    case("_resolve_extract_provider", backend="foo",
         io={"get_provider": None, "selection_exists": False, "get_active_extract_provider": {"name": "exa"}}),
    case("_resolve_extract_provider", backend="",
         io={"get_active_extract_provider": None, "_disabled_web_plugin_for": None}),
    case("_resolve_extract_provider", backend="",
         io={"get_active_extract_provider": {"name": "firecrawl", "display_name": "F"}}),
    case("_resolve_extract_provider", backend="firecrawl",
         io={"get_provider": rais("RuntimeError")}),
    case("_resolve_extract_provider", backend="foo",
         io={"get_provider": None, "selection_exists": True, "selection_error": "SELERR",
             "_disabled_web_plugin_for": "plugins/web/foo"}),
    case("_resolve_extract_provider", backend="firecrawl",
         io={"get_provider": {"name": "firecrawl", "display_name": "F"}, "provider.supports_extract": rais("SystemExit")}),
)

# ── _extract_timeout_seconds ────────────────────────────────────────────────
add(
    case("_extract_timeout_seconds", io={"_load_web_config": {"extract_timeout": "30.5"}}),
    case("_extract_timeout_seconds", io={"_load_web_config": {"extract_timeout": "abc"}}),
    case("_extract_timeout_seconds", io={"_load_web_config": {}}),
    case("_extract_timeout_seconds", io={"_load_web_config": {"extract_timeout": None}}),
    case("_extract_timeout_seconds", io={"_load_web_config": {"extract_timeout": 60}}),
    case("_extract_timeout_seconds", io={"_load_web_config": {"extract_timeout": True}}),
    case("_extract_timeout_seconds", io={"_load_web_config": rais("RuntimeError")}),
    case("_extract_timeout_seconds", io={"_load_web_config": {"extract_timeout": [1]}}),
)

# ── _backend_failed_whole_batch ─────────────────────────────────────────────
add(
    case("_backend_failed_whole_batch", results=[{"error": "e1"}, {"error": "e2"}],
         io={"is_origin_http_error": seq(False, False)}),
    case("_backend_failed_whole_batch", results=[{"error": "e1"}, {"error": None}], io={}),
    case("_backend_failed_whole_batch", results=[], io={}),
    case("_backend_failed_whole_batch", results=[{"error": "e1"}], io={"is_origin_http_error": True}),
    case("_backend_failed_whole_batch", results=[{"error": "e1"}, {"error": "e2"}],
         io={"is_origin_http_error": seq(False, True)}),
    case("_backend_failed_whole_batch", results=[{"error": "e1"}], io={"is_origin_http_error": rais("RuntimeError")}),
    case("_backend_failed_whole_batch", results=[{}], io={}),
    case("_backend_failed_whole_batch", results=[{"error": "e1"}, {"error": "e2"}], io={}),
)


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {f: n for f, n in counts.items() if n < 5}
    if thin:
        raise SystemExit(f"generator: funcs below 5 cases: {thin}")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=True) + "\n")
    print(f"gen_web_tools_flow_cases: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()
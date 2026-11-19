#!/usr/bin/env python3
"""Generate tests/fixtures/prompt_caching_flow.jsonl for the
agent/prompt_caching.py FLOW port (src/agent/prompt_caching_flow.c).

Case shape: {"func": <name>, "in": {...args..., "io": {seam: script}}}.
Scripts (shared contract with the C universal seam pc_call and the oracle):
  - MISSING key            -> fail-closed RuntimeError("no script for <name>")
  - null                   -> None result
  - {"$seq": [v1, v2, ...]}-> one result per call; exhaustion raises
                              RuntimeError("script exhausted: <name>")
  - {"__raise__": cls, "msg": m, ...} -> raises; trace ret is the WHOLE marker
  - any other value        -> reused on every call (a bare ARRAY is a LIST
                              RESULT, not a queue — wrap queues in $seq)

Seam names are the LIVE collaborator names (underscores kept):
  find_stable_prefix, strip_anthropic_cache_control (claimed sibling),
  session_source_for, finite_turn_required, active_policy, adaptive_start_tier,
  _infer_provider_from_url, get_provider_profile, fetch_endpoint_model_metadata,
  marker_preference, load_config_readonly.

get_provider_profile / active_policy results are materialized into attribute
doubles (SimpleNamespace) by the oracle; the C port reads the JSON fields.
fetch_endpoint_model_metadata / load_config_readonly / strip results stay RAW
dicts/lists (consumed via .get / [0]).

Fixture domain (documented in the port header):
  - ASCII strings only (str.strip/lower/slice transcribed byte-wise).
  - request_json is strict json.dumps-produced JSON (no JSONDecodeError paths).
  - profile.name / model / api_key / source are str-or-null; infer returns
    str-or-null; find_stable_prefix returns str-or-null.
  - _tool_run_end start is a non-negative integer (live call domain).
  - PromptCachePlan.__hash__ fields carry at least one list/dict (unhashable);
    the all-scalar hash domain (randomized str hash) is excluded.
  - strip_anthropic_cache_control results: list / null / str / dict (the
    subscript outcomes); numeric/bool results are out of domain.
  - integral floats avoided (libjson has no int/float identity); bool<->int
    cross-equality IS exercised via __eq__.
"""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "fixtures" / "prompt_caching_flow.jsonl"

M = {"type": "ephemeral"}
M1H = {"type": "ephemeral", "ttl": "1h"}


def seq(*vals):
    return {"$seq": list(vals)}


def rais(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def case(func, **inp):
    return {"func": func, "in": inp}


def prof(**kw):
    return dict(kw)


CASES = []


def add(*cases):
    CASES.extend(cases)


# ── _text_part ──────────────────────────────────────────────────────────────
add(
    case("_text_part", text="hello"),
    case("_text_part", text="hello", cache_marker=M),
    case("_text_part", text="hello", cache_marker=M1H),
    case("_text_part", text="hello", cache_marker=None),
    case("_text_part", text="hello", cache_marker=False),
    case("_text_part", text="hello", cache_marker=0),
    case("_text_part", text=""),
    case("_text_part", text="x", cache_marker=""),
)

# ── _has_part_marker ────────────────────────────────────────────────────────
add(
    case("_has_part_marker", content=None),
    case("_has_part_marker", content="str"),
    case("_has_part_marker", content=[]),
    case("_has_part_marker", content=[{"type": "text"}]),
    case("_has_part_marker", content=[{"type": "text", "cache_control": M}]),
    case("_has_part_marker", content=[5, {"cache_control": None}]),
    case("_has_part_marker", content=[{"cache_control": False}]),
    case("_has_part_marker", content={"cache_control": 1}),
)

# ── is_one_shot_request ─────────────────────────────────────────────────────
add(
    case("is_one_shot_request", messages=None, tools=None),
    case("is_one_shot_request", messages=[], tools=[]),
    case("is_one_shot_request", messages=[{"role": "user", "content": "hi"}], tools=None),
    case("is_one_shot_request", messages=[{"role": "user"}, {"role": "user"}], tools=None),
    case("is_one_shot_request", messages=[{"role": "assistant", "content": "x"}], tools=None),
    case("is_one_shot_request", messages=[{"role": "tool", "content": "x"}], tools=None),
    case("is_one_shot_request", messages=[{"role": "user", "cache_control": M}], tools=None),
    case("is_one_shot_request",
         messages=[{"role": "user", "content": [{"type": "text", "text": "x", "cache_control": {}}]}],
         tools=None),
    case("is_one_shot_request", messages=[], tools=[{"name": "t"}]),
    case("is_one_shot_request", messages=[5, "x", {"role": "user"}], tools=None),
    case("is_one_shot_request", messages={"a": 1}, tools=None),
    case("is_one_shot_request", messages="ab", tools=None),
    case("is_one_shot_request", messages=5, tools=None),
    case("is_one_shot_request", messages=[{"role": "system", "content": "s"},
                                          {"role": "user", "content": "u"}], tools=None),
    case("is_one_shot_request", messages=[{"role": "user", "content": ""}], tools=False),
)

# ── _scaffold_parts ─────────────────────────────────────────────────────────
SC_MSG = {"role": "user", "content": "SCAFFOLD tail"}
add(
    case("_scaffold_parts", msg=SC_MSG, cache_marker=M, io={"find_stable_prefix": "SCAFFOLD"}),
    case("_scaffold_parts", msg=SC_MSG, cache_marker=M, io={"find_stable_prefix": None}),
    case("_scaffold_parts", msg={"role": "assistant", "content": "SCAFFOLD tail"},
         cache_marker=M, io={"find_stable_prefix": "X"}),
    case("_scaffold_parts", msg={"role": "user", "content": 123},
         cache_marker=M, io={"find_stable_prefix": "X"}),
    case("_scaffold_parts", msg=SC_MSG, cache_marker=None, io={"find_stable_prefix": "SCAFFOLD"}),
    case("_scaffold_parts", msg=SC_MSG, cache_marker=M, io={"find_stable_prefix": ""}),
    case("_scaffold_parts", msg=SC_MSG, cache_marker=M,
         io={"find_stable_prefix": "SCAFFOLD tail extra"}),
    case("_scaffold_parts", msg=SC_MSG, cache_marker=M, io={"find_stable_prefix": 5}),
    case("_scaffold_parts", msg=SC_MSG, cache_marker=M, io={}),
    case("_scaffold_parts", msg=SC_MSG, cache_marker=M, io={"find_stable_prefix": rais("ValueError")}),
    case("_scaffold_parts", msg={"content": "no role"}, io={"find_stable_prefix": "X"}),
    case("_scaffold_parts", msg=SC_MSG, cache_marker=False, io={"find_stable_prefix": "SCAFFOLD"}),
)

# ── _split_unmarked_scaffolds ───────────────────────────────────────────────
SPLIT_MSGS = [{"role": "user", "content": "A tail"},
              {"role": "assistant", "content": "x"},
              5,
              {"role": "user", "content": "B tail"}]
add(
    case("_split_unmarked_scaffolds", messages=SPLIT_MSGS,
         io={"find_stable_prefix": seq("A", None)}),
    case("_split_unmarked_scaffolds", messages=[{"role": "user", "content": "S tail"}],
         io={"find_stable_prefix": "S"}),
    case("_split_unmarked_scaffolds", messages=[], io={"find_stable_prefix": "S"}),
    case("_split_unmarked_scaffolds",
         messages=[{"role": "user", "content": "A tail"}, {"role": "user", "content": "B tail"}],
         io={"find_stable_prefix": seq("A")}),
    case("_split_unmarked_scaffolds", messages=[{"role": "user", "content": "A tail"}],
         io={"find_stable_prefix": rais("RuntimeError")}),
    case("_split_unmarked_scaffolds", messages=[{"role": "user", "content": "A tail"}], io={}),
    case("_split_unmarked_scaffolds",
         messages=[{"role": "user", "content": "S tail", "extra": 1}],
         io={"find_stable_prefix": "S"}),
    case("_split_unmarked_scaffolds", messages=[1, "x", None], io={"find_stable_prefix": "S"}),
    case("_split_unmarked_scaffolds", messages=SPLIT_MSGS,
         io={"find_stable_prefix": seq(None, "B")}),
)

# ── _strip_prior_decoration ─────────────────────────────────────────────────
MARKED = {"role": "user", "content": "x", "cache_control": M}
MARKED2 = {"role": "assistant", "content": "y", "cache_control": M}
LIST_CONTENT = {"role": "user", "content": [{"type": "text", "text": "x"}]}
add(
    case("_strip_prior_decoration", messages=[MARKED],
         io={"strip_anthropic_cache_control": [{"role": "user", "content": "x"}]}),
    case("_strip_prior_decoration", messages=[LIST_CONTENT],
         io={"strip_anthropic_cache_control": [{"role": "user", "content": "x"}]}),
    case("_strip_prior_decoration", messages=[{"role": "user", "content": "plain"}],
         io={"strip_anthropic_cache_control": [{"role": "user", "content": "z"}]}),
    case("_strip_prior_decoration", messages=[5, "x", None, {"role": "system", "content": "s"}],
         io={"strip_anthropic_cache_control": [{"role": "user", "content": "z"}]}),
    case("_strip_prior_decoration", messages=[MARKED, MARKED2],
         io={"strip_anthropic_cache_control": seq([{"role": "user", "content": "a"}],
                                                  [{"role": "assistant", "content": "b"}])}),
    case("_strip_prior_decoration", messages=[MARKED], io={"strip_anthropic_cache_control": []}),
    case("_strip_prior_decoration", messages=[MARKED], io={"strip_anthropic_cache_control": None}),
    case("_strip_prior_decoration", messages=[MARKED],
         io={"strip_anthropic_cache_control": rais("ValueError")}),
    case("_strip_prior_decoration", messages=[MARKED, MARKED2],
         io={"strip_anthropic_cache_control": [{"role": "user", "content": "a"}]}),
    case("_strip_prior_decoration", messages=[MARKED], io={}),
    case("_strip_prior_decoration", messages=[MARKED], io={"strip_anthropic_cache_control": "abc"}),
    case("_strip_prior_decoration", messages=[MARKED], io={"strip_anthropic_cache_control": {}}),
    case("_strip_prior_decoration", messages=[MARKED, MARKED2],
         io={"strip_anthropic_cache_control": seq([{"role": "user", "content": "a"}])}),
)

# ── auto_cache_ttl_for_source ───────────────────────────────────────────────
add(
    case("auto_cache_ttl_for_source", source="cron"),
    case("auto_cache_ttl_for_source", source=" Cron "),
    case("auto_cache_ttl_for_source", source="CRON"),
    case("auto_cache_ttl_for_source", source="cli"),
    case("auto_cache_ttl_for_source", source=None),
    case("auto_cache_ttl_for_source", source=""),
    case("auto_cache_ttl_for_source", source="api_server"),
    case("auto_cache_ttl_for_source", source="gateway_hygiene"),
    case("auto_cache_ttl_for_source", source="subagent"),
    case("auto_cache_ttl_for_source", source="desktop"),
    case("auto_cache_ttl_for_source", source="cron\x1c"),
    case("auto_cache_ttl_for_source", source=5),
    case("auto_cache_ttl_for_source", source="msgraph_webhook"),
)

# ── auto_cache_ttl_for_agent ────────────────────────────────────────────────
AG = {"platform": "cli"}
POL_ONESHOT = {"request_json": "{}", "source": "oneshot"}
POL_CLI = {"request_json": "{}", "source": "cli"}
add(
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": True, "active_policy": None}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": False, "active_policy": None, "session_source_for": "cli"}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None, "active_policy": None, "session_source_for": "cron"}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None,
             "active_policy": {"request_json": '{"turn_v1":{"finite":true}}', "source": "cli"}}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None,
             "active_policy": {"request_json": '{"turn_v1":{"finite":false}}', "source": "cli"},
             "session_source_for": "cli"}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None, "active_policy": POL_ONESHOT}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None, "active_policy": POL_CLI, "session_source_for": "tui"}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None,
             "active_policy": {"request_json": '{"turn_v1":null}', "source": "cli"}}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None,
             "active_policy": {"request_json": '{"turn_v1":{"finite":true}}'}}),
    case("auto_cache_ttl_for_agent", agent=AG, io={"finite_turn_required": rais("RuntimeError")}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None, "active_policy": rais("ValueError")}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None, "active_policy": None,
             "session_source_for": rais("KeyError")}),
    case("auto_cache_ttl_for_agent", agent=AG, io={}),
    case("auto_cache_ttl_for_agent", agent=None, io={}),
    case("auto_cache_ttl_for_agent", agent={"platform": None},
         io={"finite_turn_required": False, "active_policy": None, "session_source_for": "cli"}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": 1, "active_policy": None, "session_source_for": "cli"}),
    case("auto_cache_ttl_for_agent", agent=AG,
         io={"finite_turn_required": None,
             "active_policy": {"request_json": '{"turn_v1":{"finite":1}}', "source": "cli"},
             "session_source_for": "cli"}),
)

# ── auto_start_cache_ttl ────────────────────────────────────────────────────
add(
    case("auto_start_cache_ttl", agent=AG,
         io={"finite_turn_required": False, "active_policy": None,
             "session_source_for": "cli", "adaptive_start_tier": "5m"}),
    case("auto_start_cache_ttl", agent=AG,
         io={"finite_turn_required": True, "active_policy": None, "adaptive_start_tier": "1h"}),
    case("auto_start_cache_ttl", agent=AG,
         io={"finite_turn_required": False, "active_policy": None,
             "session_source_for": "cli", "adaptive_start_tier": rais("RuntimeError")}),
    case("auto_start_cache_ttl", agent=None, io={}),
    case("auto_start_cache_ttl", agent=AG,
         io={"finite_turn_required": False, "active_policy": None, "session_source_for": "cli"}),
    case("auto_start_cache_ttl", agent=AG,
         io={"finite_turn_required": None, "active_policy": POL_ONESHOT,
             "adaptive_start_tier": "5m"}),
    case("auto_start_cache_ttl", agent=AG,
         io={"finite_turn_required": False, "active_policy": None,
             "session_source_for": "cron", "adaptive_start_tier": "5m"}),
)

# ── refresh_auto_cache_ttl ──────────────────────────────────────────────────
R_AG = {"_cache_ttl_auto": True, "platform": "cli"}
R_UP = {"_cache_ttl_auto": True, "_cache_ttl_adaptive": True, "_cache_ttl": "1h", "platform": "cli"}
R_IO = {"finite_turn_required": False, "active_policy": None,
        "session_source_for": "cli", "adaptive_start_tier": "5m"}
add(
    case("refresh_auto_cache_ttl", agent={}, io={}),
    case("refresh_auto_cache_ttl", agent={"_cache_ttl_auto": False}, io={}),
    case("refresh_auto_cache_ttl", agent={"_cache_ttl_auto": True, "side_agent": True}, io={}),
    case("refresh_auto_cache_ttl", agent=R_AG, io=R_IO),
    case("refresh_auto_cache_ttl", agent=R_UP, io=R_IO),
    case("refresh_auto_cache_ttl",
         agent={"_cache_ttl_auto": True, "_cache_ttl_adaptive": True, "_cache_ttl": "5m",
                "platform": "cli"}, io=R_IO),
    case("refresh_auto_cache_ttl",
         agent={"_cache_ttl_auto": True, "_cache_ttl_adaptive": True, "platform": "cli"}, io={}),
    case("refresh_auto_cache_ttl",
         agent={"_cache_ttl_auto": 1, "_cache_ttl_adaptive": 1, "_cache_ttl": "1h",
                "platform": "cli"}, io=R_IO),
    case("refresh_auto_cache_ttl", agent=R_AG,
         io={"finite_turn_required": False, "active_policy": None,
             "session_source_for": "cli", "adaptive_start_tier": rais("RuntimeError")}),
    case("refresh_auto_cache_ttl", agent=None, io={}),
    case("refresh_auto_cache_ttl", agent={"_cache_ttl_auto": True, "side_agent": 1}, io={}),
    case("refresh_auto_cache_ttl", agent=R_AG, io={}),
)

# ── _route_names ────────────────────────────────────────────────────────────
add(
    case("_route_names", provider="nous", base_url="",
         io={"get_provider_profile": {"name": "nous"}}),
    case("_route_names", provider="custom", base_url="https://portal.nousresearch.com/v1",
         io={"_infer_provider_from_url": "nous",
             "get_provider_profile": seq({"name": "custom"}, {"name": "nous"})}),
    case("_route_names", provider="", base_url="https://x",
         io={"_infer_provider_from_url": "nous", "get_provider_profile": {"name": "nous"}}),
    case("_route_names", provider="nous", base_url="https://x",
         io={"_infer_provider_from_url": None, "get_provider_profile": {"name": "nous"}}),
    case("_route_names", provider="nous", base_url="https://x",
         io={"_infer_provider_from_url": "nous", "get_provider_profile": {"name": "nous"}}),
    case("_route_names", provider="nous", base_url="", io={"get_provider_profile": None}),
    case("_route_names", provider="nous", base_url="", io={"get_provider_profile": {"name": "alias"}}),
    case("_route_names", provider="nous", base_url="", io={"get_provider_profile": {"name": ""}}),
    case("_route_names", provider=None, base_url="", io={}),
    case("_route_names", provider=" Nous ", base_url="", io={"get_provider_profile": {"name": "nous"}}),
    case("_route_names", provider="nous", base_url="https://x", io={}),
    case("_route_names", provider=5, base_url="", io={}),
    case("_route_names", provider="nous", base_url="",
         io={"get_provider_profile": rais("ValueError")}),
    case("_route_names", provider="a", base_url="https://x",
         io={"_infer_provider_from_url": "b",
             "get_provider_profile": seq({"name": "a2"}, {"name": "b2"})}),
)

# ── _catalog_route ──────────────────────────────────────────────────────────
CAT_T = {"prompt_cache_markers_from_catalog": True}
CAT_F = {"prompt_cache_markers_from_catalog": False}
add(
    case("_catalog_route", provider="nous", base_url="", io={"get_provider_profile": CAT_T}),
    case("_catalog_route", provider="nous", base_url="", io={"get_provider_profile": CAT_F}),
    case("_catalog_route", provider="nous", base_url="", io={"get_provider_profile": None}),
    case("_catalog_route", provider="", base_url="", io={}),
    case("_catalog_route", provider="a", base_url="https://x",
         io={"_infer_provider_from_url": "b", "get_provider_profile": CAT_T}),
    case("_catalog_route", provider="nous", base_url="",
         io={"get_provider_profile": rais("RuntimeError")}),
    case("_catalog_route", provider="nous", base_url="", io={}),
    case("_catalog_route", provider="a", base_url="https://x",
         io={"_infer_provider_from_url": "b",
             "get_provider_profile": seq(CAT_T, CAT_F, CAT_T)}),
)

# ── catalog_cache_markers ───────────────────────────────────────────────────
CAT_PROF = {"prompt_cache_markers_from_catalog": True, "base_url": "https://api"}
ROW_OK = {"m": {"pricing": {"cache_write": "0.001"}}}
add(
    case("catalog_cache_markers", provider="nous", base_url="", model="m",
         io={"get_provider_profile": {}}),
    case("catalog_cache_markers", provider="nous", base_url="", model="",
         io={"get_provider_profile": CAT_PROF}),
    case("catalog_cache_markers", provider="nous", base_url="", model="m", api_key="",
         io={"get_provider_profile": CAT_PROF, "fetch_endpoint_model_metadata": ROW_OK}),
    case("catalog_cache_markers", provider="nous", base_url="https://override", model="m",
         io={"get_provider_profile": CAT_PROF, "fetch_endpoint_model_metadata": ROW_OK}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": {"m": {"pricing": {"cache_write": "0"}}}}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": {"m": {"pricing": {"cache_write": ""}}}}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": {"m": {"pricing": {"cache_write": None}}}}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": {"m": {"pricing": {"cache_write": 0}}}}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": {"m": {"pricing": {}}}}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF, "fetch_endpoint_model_metadata": {"m": "row"}}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": {"m": {"pricing": "x"}}}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF, "fetch_endpoint_model_metadata": None}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF, "fetch_endpoint_model_metadata": {"other": {}}}),
    case("catalog_cache_markers", provider="nous", base_url="", model="m",
         io={"get_provider_profile": {"prompt_cache_markers_from_catalog": True}}),
    case("catalog_cache_markers", provider="nous", base_url="", model="m",
         io={"get_provider_profile": {"prompt_cache_markers_from_catalog": True, "base_url": None},
             "fetch_endpoint_model_metadata": {"m": {"pricing": {"cache_write": "1"}}}}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": rais("RuntimeError")}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m",
         io={"get_provider_profile": CAT_PROF}),
    case("catalog_cache_markers", provider="nous", base_url="https://o", model="m", api_key="sk",
         io={"get_provider_profile": CAT_PROF, "fetch_endpoint_model_metadata": ROW_OK}),
    case("catalog_cache_markers", provider="a", base_url="https://x", model="m",
         io={"_infer_provider_from_url": "b",
             "get_provider_profile": seq({}, CAT_PROF, {}, CAT_PROF),
             "fetch_endpoint_model_metadata": {"m": {"pricing": {"cache_write": "1"}}}}),
)

# ── learned_or_catalog_markers ──────────────────────────────────────────────
add(
    case("learned_or_catalog_markers", provider="nous", base_url="https://o", model="m",
         io={"marker_preference": True}),
    case("learned_or_catalog_markers", provider="nous", base_url="https://o", model="m",
         io={"marker_preference": False}),
    case("learned_or_catalog_markers", provider="nous", base_url="", model="m",
         io={"marker_preference": None, "get_provider_profile": {}}),
    case("learned_or_catalog_markers", provider="nous", base_url="https://o", model="m",
         io={"marker_preference": None, "get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": {"m": {"pricing": {"cache_write": "1"}}}}),
    case("learned_or_catalog_markers", provider="nous", base_url="https://o", model="m",
         io={"marker_preference": rais("ValueError")}),
    case("learned_or_catalog_markers", provider="nous", base_url="https://o", model="m", io={}),
    case("learned_or_catalog_markers", provider="nous", base_url="https://o", model="m",
         io={"marker_preference": None, "get_provider_profile": CAT_PROF,
             "fetch_endpoint_model_metadata": None}),
    case("learned_or_catalog_markers", provider="nous", base_url="https://o", model="m",
         io={"marker_preference": 0}),
)

# ── route_ttl_ceiling ───────────────────────────────────────────────────────
def cfg(ceilings):
    return {"prompt_caching": {"route_ttl_ceilings": ceilings}}


add(
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {}, "load_config_readonly": cfg({"nous": "5m"})}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {}, "load_config_readonly": cfg({"nous": "2h"})}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {}, "load_config_readonly": cfg({"nous": None})}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {}, "load_config_readonly": cfg({"nous": "1h"})}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {"prompt_cache_max_ttl": "1h"},
             "load_config_readonly": {"prompt_caching": "x"}}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {"prompt_cache_max_ttl": "5m"},
             "load_config_readonly": {}}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {"prompt_cache_max_ttl": "2h"},
             "load_config_readonly": {}}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": None, "load_config_readonly": {}}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {"prompt_cache_max_ttl": "1h"},
             "load_config_readonly": {"prompt_caching": {"route_ttl_ceilings": "x"}}}),
    case("route_ttl_ceiling", provider="a", base_url="https://x",
         io={"_infer_provider_from_url": "b",
             "get_provider_profile": seq({"name": "a"}, {"name": "b"}),
             "load_config_readonly": cfg({"b": "5m"})}),
    case("route_ttl_ceiling", provider="a", base_url="https://x",
         io={"_infer_provider_from_url": "b",
             "get_provider_profile": seq({"name": "a", "prompt_cache_max_ttl": "2h"},
                                         {"name": "b", "prompt_cache_max_ttl": "1h"},
                                         {"name": "a", "prompt_cache_max_ttl": "2h"},
                                         {"name": "b", "prompt_cache_max_ttl": "1h"}),
             "load_config_readonly": {}}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {}, "load_config_readonly": rais("RuntimeError")}),
    case("route_ttl_ceiling", provider="nous", base_url="", io={"get_provider_profile": {}}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {}, "load_config_readonly": None}),
    case("route_ttl_ceiling", provider="nous", base_url="",
         io={"get_provider_profile": {}, "load_config_readonly": cfg({"nous": 5})}),
)

# ── _tool_run_end (nested closure of _completed_transaction_endpoint_indexes) ──
TR = [{"role": "tool"}, {"role": "tool"}, {"role": "user"}]
add(
    case("_tool_run_end", messages=TR, start=0),
    case("_tool_run_end", messages=TR, start=1),
    case("_tool_run_end", messages=TR, start=2),
    case("_tool_run_end", messages=TR, start=3),
    case("_tool_run_end", messages=TR, start=5),
    case("_tool_run_end", messages=[{"role": "tool"}, 5, {"role": "tool"}], start=0),
    case("_tool_run_end", messages=[{"role": "tool"}, {"role": "assistant"}], start=0),
    case("_tool_run_end", messages=[{}, {"role": "tool"}], start=0),
    case("_tool_run_end", messages=[], start=0),
    case("_tool_run_end", messages=[{"role": "tool"}], start=0),
)

# ── PromptCachePlan.__init__ ────────────────────────────────────────────────
add(
    case("PromptCachePlan.__init__", kwargs={"messages": [1], "tools": [2]}),
    case("PromptCachePlan.__init__", args=[[1], [2]]),
    case("PromptCachePlan.__init__", args=[[1]], kwargs={"tools": [2]}),
    case("PromptCachePlan.__init__", kwargs={"tools": [2], "messages": [1]}),
    case("PromptCachePlan.__init__", args=[], kwargs={}),
    case("PromptCachePlan.__init__", kwargs={"messages": []}),
    case("PromptCachePlan.__init__", kwargs={"messages": [], "tools": [], "zz": 1}),
    case("PromptCachePlan.__init__", args=[[], []], kwargs={"messages": []}),
    case("PromptCachePlan.__init__", args=[[], [], []]),
    case("PromptCachePlan.__init__", kwargs={"messages": None, "tools": None}),
)

# ── PromptCachePlan.__eq__ ──────────────────────────────────────────────────
def eqcase(self_f, other):
    return case("PromptCachePlan.__eq__", **{"self": self_f, "other": other})


SF = {"messages": [{"role": "user"}], "tools": []}
add(
    eqcase(SF, {"kind": "plan", "fields": {"messages": [{"role": "user"}], "tools": []}}),
    eqcase(SF, {"kind": "plan", "fields": {"messages": [], "tools": []}}),
    eqcase(SF, {"kind": "plan", "fields": {"messages": [{"role": "user"}], "tools": [1]}}),
    eqcase(SF, {"kind": "object"}),
    eqcase(SF, {"kind": "subclass", "fields": SF}),
    eqcase({"messages": [True], "tools": []}, {"kind": "plan", "fields": {"messages": [1], "tools": []}}),
    eqcase({"messages": [0], "tools": []}, {"kind": "plan", "fields": {"messages": [False], "tools": []}}),
    eqcase({"messages": [""], "tools": []}, {"kind": "plan", "fields": {"messages": [False], "tools": []}}),
    eqcase({"messages": [{"a": 1, "b": 2}], "tools": []},
           {"kind": "plan", "fields": {"messages": [{"b": 2, "a": 1}], "tools": []}}),
    eqcase({"messages": [[1, 2]], "tools": []},
           {"kind": "plan", "fields": {"messages": [[2, 1]], "tools": []}}),
)

# ── PromptCachePlan.__setattr__ / __delattr__ / __hash__ ────────────────────
PF = {"messages": [], "tools": []}
add(
    case("PromptCachePlan.__setattr__", fields=PF, name="messages", value=[]),
    case("PromptCachePlan.__setattr__", fields=PF, name="tools", value=None),
    case("PromptCachePlan.__setattr__", fields=PF, name="extra", value=1),
    case("PromptCachePlan.__setattr__", fields=PF, name="", value={}),
    case("PromptCachePlan.__setattr__", fields=PF, name="zzz", value="x"),
    case("PromptCachePlan.__delattr__", fields=PF, name="messages"),
    case("PromptCachePlan.__delattr__", fields=PF, name="tools"),
    case("PromptCachePlan.__delattr__", fields=PF, name="nope"),
    case("PromptCachePlan.__delattr__", fields=PF, name=""),
    case("PromptCachePlan.__delattr__", fields=PF, name="extra"),
    case("PromptCachePlan.__hash__", fields={"messages": [1], "tools": []}),
    case("PromptCachePlan.__hash__", fields={"messages": {}, "tools": []}),
    case("PromptCachePlan.__hash__", fields={"messages": None, "tools": {}}),
    case("PromptCachePlan.__hash__", fields={"messages": [], "tools": None}),
    case("PromptCachePlan.__hash__", fields={"messages": [{"a": 1}], "tools": [{"b": 2}]}),
    case("PromptCachePlan.__hash__", fields={"messages": None, "tools": [1]}),
)


def main():
    # _route_names fires _infer_provider_from_url whenever base_url is truthy;
    # catalog/learned cases with a truthy base_url must script it (None = no
    # inferred second name) or they fail-closed before the fetch path.
    for c in CASES:
        if c["func"] in ("catalog_cache_markers", "learned_or_catalog_markers"):
            inp = c["in"]
            if inp.get("base_url"):
                inp.setdefault("io", {}).setdefault("_infer_provider_from_url", None)
    # Per-function coverage floor (>=5) and func-set sanity.
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
    print(f"gen_prompt_caching_flow_cases: {len(CASES)} cases, {len(counts)} funcs -> {OUT}")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")


if __name__ == "__main__":
    main()

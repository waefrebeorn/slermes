#!/usr/bin/env python3
"""Generate tests/fixtures/context_compressor_tail_flow2.jsonl — the scripted-seam
case corpus for the context_compressor_tail_flow2 FLOW + leaf port (12 funcs,
>=5 cases each). ASCII scope (the only non-ASCII is the byte-pinned U+2014 em
dash); integral floats == ints. Covers normal paths, branch edges, short-circuit
order, negative-index resolution, real scripted exceptions (incl. the
BaseException trio + CancelledError), and fail-closed missing / exhausted
scripts. Run: python tests/gen_context_compressor_tail_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "context_compressor_tail_flow2.jsonl")

EM = "\u2014"  # U+2014 em dash
RULE = ("## Skill Safety Rule\n"
        "A skill placeholder containing `[SKILL_PRUNED]` lost its content in context compression and is inaccessible " + EM + " "
        "reload it with skill_view(name='...') before acting on anything that depends on it. After reloading, ignore any "
        "remaining `[SKILL_PRUNED]` markers for that same skill; they are historical artifacts of earlier compactions.")
SUFFIX = "\n\n" + RULE
PREFIX = "[SKILL_PRUNED:"
STUB = "[read_file output demoted at compaction " + EM + " 1,500 chars preserved in session history.]"
STUB_HINT = ("[tool output demoted at compaction " + EM + " 1500 chars preserved in session history. "
             "Recover with session_search(query=..., session_id='s1')]")
LONG = "x" * 1600          # >= _LEAN_TAIL_DEMOTE_MIN_CHARS (1500)
LONG_RULE = LONG + SUFFIX   # long content carrying the ribbon

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def msg(role, **kw):
    d = {"role": role}
    d.update(kw)
    return d


def raises(kind, message=""):
    return {"__raise__": kind, "message": message}


def attr(**kw):
    return {"__attr__": kw}


def callable_(seam):
    return {"__callable__": seam}


def script(ret):
    return {"__script__": True, "ret": ret}


def tc(id_, name, arguments):
    return {"id": id_, "function": {"name": name, "arguments": arguments}}


# ── lean_tail_budget (pure arithmetic, no seams) ──
add("lean_tail_budget", context_length=400000, threshold_tokens=12000)
add("lean_tail_budget", context_length=1000000, threshold_tokens=0)
add("lean_tail_budget", context_length=100, threshold_tokens=100)
add("lean_tail_budget", context_length=400000, threshold_tokens=2000)
add("lean_tail_budget", context_length=400000, threshold_tokens=2001)
add("lean_tail_budget", context_length=0, threshold_tokens=-5)
add("lean_tail_budget", context_length=1000000, threshold_tokens=100000)
add("lean_tail_budget", context_length=True, threshold_tokens=True)
add("lean_tail_budget", context_length=400000.0, threshold_tokens=12000.0)
add("lean_tail_budget", context_length=-40000, threshold_tokens=0)
add("lean_tail_budget", context_length="x", threshold_tokens=100)
add("lean_tail_budget", context_length=None, threshold_tokens=100)
add("lean_tail_budget", context_length=400000, threshold_tokens="x")
add("lean_tail_budget", context_length=400000, threshold_tokens=None)
add("lean_tail_budget", context_length=[], threshold_tokens=100)

# ── is_demoted_stub ──
add("is_demoted_stub", content=STUB)
add("is_demoted_stub", content=STUB_HINT)
add("is_demoted_stub", content="plain text", io={"has_archive_pointer": False})
add("is_demoted_stub", content="plain text", io={"has_archive_pointer": True})
add("is_demoted_stub", content="x" * 1500, io={"has_archive_pointer": False})
add("is_demoted_stub", content=None)
add("is_demoted_stub", content=5)
add("is_demoted_stub", content=True)
add("is_demoted_stub", content=[])
add("is_demoted_stub", content=["x"] * 1000, io={"has_archive_pointer": False})
add("is_demoted_stub", content="[x output demoted at compaction " + EM + " 1,500 chars preserved in session history!]",
    io={"has_archive_pointer": False})
add("is_demoted_stub", content=STUB, io={"has_archive_pointer": raises("RuntimeError", "boom")})
add("is_demoted_stub", content="plain text")  # fail-closed
add("is_demoted_stub", content="[a b output demoted at compaction " + EM + " 1,500 chars preserved in session history.]",
    io={"has_archive_pointer": False})
add("is_demoted_stub", content="plain", io={"has_archive_pointer": raises("KeyboardInterrupt")})
add("is_demoted_stub", content="[x\x1coutput demoted at compaction " + EM + " 1,500 chars preserved in session history.]",
    io={"has_archive_pointer": False})

# ── skill_view_names_by_call_id ──
FN_ALPHA = {"name": "skill_view", "arguments": '{"name":"alpha"}'}
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":"alpha"}')])],
    io={"_tc_get": [FN_ALPHA, "skill_view", '{"name":"alpha"}', "c1"],
        "_json_dict": {"name": "alpha"}})
add("skill_view_names_by_call_id", messages=[msg("user", content="hi")])
add("skill_view_names_by_call_id", messages=[msg("assistant", tool_calls=[])])
add("skill_view_names_by_call_id", messages=[msg("assistant", tool_calls=None)])
add("skill_view_names_by_call_id", messages=[msg("assistant")])
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc("c1", "other", "{}")])],
    io={"_tc_get": [{"name": "other"}, "other"]})
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":""}')])],
    io={"_tc_get": [FN_ALPHA, "skill_view", '{"name":""}'], "_json_dict": {"name": ""}})
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":5}')])],
    io={"_tc_get": [FN_ALPHA, "skill_view", '{"name":5}'], "_json_dict": {"name": 5}})
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc(None, "skill_view", '{"name":"alpha"}')])],
    io={"_tc_get": [FN_ALPHA, "skill_view", '{"name":"alpha"}', None], "_json_dict": {"name": "alpha"}})
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc(5, "skill_view", '{"name":"alpha"}')])],
    io={"_tc_get": [FN_ALPHA, "skill_view", '{"name":"alpha"}', 5], "_json_dict": {"name": "alpha"}})
add("skill_view_names_by_call_id", messages=[None])
add("skill_view_names_by_call_id", messages=5)
add("skill_view_names_by_call_id", messages=None)
add("skill_view_names_by_call_id", messages="ab")
add("skill_view_names_by_call_id", messages={"k": 1})
add("skill_view_names_by_call_id", messages="")
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":"alpha"}')])],
    io={"_tc_get": [FN_ALPHA, "skill_view", '{"name":"alpha"}', "c1"], "_json_dict": None})
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":"alpha"}')])],
    io={"_tc_get": [FN_ALPHA, "skill_view"]})  # exhausted -> fail-closed on arguments
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=5)])
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":"alpha"}'),
                                           tc("c2", "skill_view", '{"name":"beta"}')])],
    io={"_tc_get": [FN_ALPHA, "skill_view", '{"name":"alpha"}', "c1",
                    {"name": "skill_view", "arguments": '{"name":"beta"}'}, "skill_view", '{"name":"beta"}', "c2"],
        "_json_dict": [{"name": "alpha"}, {"name": "beta"}]})
add("skill_view_names_by_call_id",
    messages=[msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":"alpha"}')])],
    io={"_tc_get": [raises("SystemExit", "0") ]})

# ── demote_stale_tail_tools ──
def tool_rounds(n):
    """n distinct tool rounds separated by user rows (tool at even indices)."""
    rows = []
    for r in range(n):
        rows.append(msg("tool", content=LONG, tool_name="read_file", tool_call_id="t%d" % r))
        rows.append(msg("user", content="u"))
    return rows


ARCH = script([[{"role": "tool", "content": "archived"}], [], 0])
RW = {"role": "tool", "content": "STUB"}
add("demote_stale_tail_tools", messages=tool_rounds(7), tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB", "_rewritten": RW,
        "archive_compaction_drops": ARCH})
add("demote_stale_tail_tools", messages=tool_rounds(6), tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB", "_rewritten": RW})
add("demote_stale_tail_tools", messages=tool_rounds(8), tail_start=0, session_id="",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB", "_rewritten": RW,
        "archive_compaction_drops": script([[{"role": "tool"}], [], 1])})
add("demote_stale_tail_tools", messages=[msg("tool", content="short")], tail_start=0, session_id="s1")
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG_RULE)], tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB", "_rewritten": RW,
        "archive_compaction_drops": ARCH})
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG + PREFIX + " alpha]")], tail_start=0, session_id="s1")
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start=0, session_id="s1",
    io={"_is_summary_stub": True})
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG, tool_call_id="c1"),
                                         msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":"alpha"}')])],
    tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB",
        "_tc_get": [FN_ALPHA, "skill_view", '{"name":"alpha"}', "c1"], "_json_dict": {"name": "alpha"},
        "_result_failure_suffix": "", "_skill_pruned_marker": PREFIX + " content lost; reload with skill_view(name='alpha')]",
        "_rewritten": RW, "archive_compaction_drops": ARCH})
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG, tool_call_id="c1"),
                                         msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":"alpha"}')])],
    tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB",
        "_tc_get": [FN_ALPHA, "skill_view", '{"name":"alpha"}', "c1"], "_json_dict": {"name": "alpha"},
        "_result_failure_suffix": " FAILED: err", "_rewritten": RW, "archive_compaction_drops": ARCH})
add("demote_stale_tail_tools", messages=None, tail_start=0, session_id="s1")
add("demote_stale_tail_tools", messages=5, tail_start=0, session_id="s1")
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start="x", session_id="s1")
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB", "_rewritten": RW,
        "archive_compaction_drops": script([[], []])})  # unpack 3 got 2
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB", "_rewritten": RW,
        "archive_compaction_drops": script(None)})  # cannot unpack NoneType
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB", "_rewritten": RW,
        "archive_compaction_drops": script([[], [], "x"])})  # -= int str
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": 5, "_rewritten": RW,
        "archive_compaction_drops": ARCH})  # stub int += -> TypeError (no skill)
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start=0, session_id="s1",
    io={"_is_summary_stub": raises("GeneratorExit")})
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start=0, session_id="s1")  # fail-closed
add("demote_stale_tail_tools", messages=[None, msg("tool", content=LONG)], tail_start=0, session_id="s1")
add("demote_stale_tail_tools", messages=tool_rounds(7), tail_start=-2, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB", "_rewritten": RW,
        "archive_compaction_drops": ARCH})
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG)], tail_start=-5, session_id="s1")  # IndexError
add("demote_stale_tail_tools", messages=[msg("tool", content=LONG, tool_call_id="c1"),
                                         msg("assistant", tool_calls=[tc("c1", "skill_view", '{"name":"alpha"}')])],
    tail_start=0, session_id="s1",
    io={"_is_summary_stub": False, "_lean_recovery_stub": "STUB",
        "_tc_get": [FN_ALPHA, "skill_view", '{"name":"alpha"}', "c1"], "_json_dict": {"name": "alpha"},
        "_result_failure_suffix": "", "_skill_pruned_marker": 5, "_rewritten": RW,
        "archive_compaction_drops": ARCH})  # " " + int -> can only concatenate str

# ── native_anthropic_tail_prices (outer + inline prices) ──
COMP = attr(api_mode="anthropic_messages", base_url="https://api.anthropic.com", model="claude",
            _session_db=None, _session_id="s1", _persistence=None)
ROW = msg("assistant", content="hi")
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW],
    io={"native_anthropic_preserves_prior_thinking": True, "session_rejected_thinking": [],
        "native_anthropic_row_costs": script([[ROW, 0]]), "_estimate_msg_budget_tokens": 10})
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW],
    io={"native_anthropic_preserves_prior_thinking": True, "session_rejected_thinking": ["sig1"],
        "strip_rejected_thinking": 1, "native_anthropic_row_costs": script([[ROW, 5]]),
        "_estimate_msg_budget_tokens": 10})
add("native_anthropic_tail_prices", compressor=attr(api_mode="openai"), messages=[ROW])
add("native_anthropic_tail_prices", compressor=None, messages=[ROW])
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW],
    io={"native_anthropic_preserves_prior_thinking": False})
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW],
    io={"native_anthropic_preserves_prior_thinking": True, "session_rejected_thinking": [],
        "native_anthropic_row_costs": script([[ROW, 0], ["non-dict", 0]]), "_estimate_msg_budget_tokens": 7})
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW],
    io={"native_anthropic_preserves_prior_thinking": True, "session_rejected_thinking": [],
        "native_anthropic_row_costs": script([[ROW]])})  # unpack 2 got 1
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW],
    io={"native_anthropic_preserves_prior_thinking": True, "session_rejected_thinking": [],
        "native_anthropic_row_costs": script(None)})  # not iterable
add("native_anthropic_tail_prices", compressor=COMP, messages=None,
    io={"native_anthropic_preserves_prior_thinking": True, "session_rejected_thinking": []})
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW],
    io={"native_anthropic_preserves_prior_thinking": raises("CancelledError")})
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW])  # fail-closed
add("native_anthropic_tail_prices", compressor=COMP, messages=[ROW],
    io={"native_anthropic_preserves_prior_thinking": True, "session_rejected_thinking": [],
        "native_anthropic_row_costs": script([[ROW, 0]]), "_estimate_msg_budget_tokens": "x"})  # + str

# ── prices (standalone nested closure; free var rejected explicit) ──
add("prices", rejected=[], messages=[ROW],
    io={"native_anthropic_row_costs": script([[ROW, 0]]), "_estimate_msg_budget_tokens": 10})
add("prices", rejected=["sig"], messages=[ROW],
    io={"strip_rejected_thinking": 1, "native_anthropic_row_costs": script([[ROW, 3]]),
        "_estimate_msg_budget_tokens": 10})
add("prices", rejected=None, messages=[ROW],
    io={"native_anthropic_row_costs": script([[ROW, 0]]), "_estimate_msg_budget_tokens": 4})
add("prices", rejected=[], messages=[ROW, "plain", None],
    io={"native_anthropic_row_costs": script([[ROW, 0], ["plain", 0], [None, 0]]),
        "_estimate_msg_budget_tokens": 4})
add("prices", rejected=[], messages=None)
add("prices", rejected=[], messages=[ROW],
    io={"native_anthropic_row_costs": script([[ROW, 0, 9]])})  # unpack 2 got 3
add("prices", rejected=["sig"], messages=[ROW],
    io={"strip_rejected_thinking": raises("SystemExit", "3"), "native_anthropic_row_costs": script([[ROW, 0]])})
add("prices", rejected=[], messages=[ROW],
    io={"native_anthropic_row_costs": script([[attr(role="assistant"), 0]])})  # __attr__ shadow -> 0
add("prices", rejected=[], messages=[ROW])  # fail-closed

# ── tail_cut_pricer (outer + lambda driven at probe) ──
TC_COMP = attr(_tail_row_pricer=callable_("compressor._tail_row_pricer"),
               _is_context_summary_message=callable_("compressor._is_context_summary_message"))
MSGS = [msg("user", content="a"), msg("assistant", content="summary"), msg("tool", content="b")]
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end=0, probe=2,
    io={"compressor._tail_row_pricer": callable_("tail_price"),
        "compressor._is_context_summary_message": [False, True, False], "tail_price": 42})
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end=0, probe=1,
    io={"compressor._tail_row_pricer": callable_("tail_price"),
        "compressor._is_context_summary_message": [False, True, False], "tail_price": 42})
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end=0, probe=2,
    io={"compressor._tail_row_pricer": callable_("tail_price"),
        "compressor._is_context_summary_message": [False, False, False], "tail_price": 9})
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end=0, probe=0,
    io={"compressor._tail_row_pricer": None, "compressor._is_context_summary_message": [False, True, False]})
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end=0, probe=2,
    io={"compressor._tail_row_pricer": None, "compressor._is_context_summary_message": [False, True, False]})
add("tail_cut_pricer", compressor=None, messages=MSGS, head_end=0, probe=0)
add("tail_cut_pricer", compressor=attr(), messages=MSGS, head_end=0, probe=0)
add("tail_cut_pricer", compressor=TC_COMP, messages=None, head_end=0, probe=0,
    io={"compressor._tail_row_pricer": callable_("tail_price")})
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end="x", probe=0,
    io={"compressor._tail_row_pricer": callable_("tail_price")})
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end=0, probe=2,
    io={"compressor._tail_row_pricer": callable_("tail_price"),
        "compressor._is_context_summary_message": [False, False, False]})  # fail-closed tail_price
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end=0, probe="x",
    io={"compressor._tail_row_pricer": callable_("tail_price"),
        "compressor._is_context_summary_message": [False, True, False]})
add("tail_cut_pricer", compressor=TC_COMP, messages=MSGS, head_end=0, probe=2,
    io={"compressor._tail_row_pricer": callable_("tail_price"),
        "compressor._is_context_summary_message": [False, True, False], "tail_price": raises("KeyboardInterrupt")})
add("tail_cut_pricer", compressor=attr(_tail_row_pricer=5,
                                       _is_context_summary_message=callable_("compressor._is_context_summary_message")),
    messages=MSGS, head_end=0, probe=0)

# ── with_skill_safety_rule ──
add("with_skill_safety_rule", summary="sum", head=[msg("user", content=PREFIX + " a]")], tail=[],
    io={"_content_text_for_contains": PREFIX + " a]"})
add("with_skill_safety_rule", summary="sum", head=[msg("user", content="plain")], tail=[],
    io={"_content_text_for_contains": "plain"})
add("with_skill_safety_rule", summary=RULE, head=[], tail=[], io={})
add("with_skill_safety_rule", summary="sum", head=[msg("user", content=LONG_RULE)], tail=[],
    io={"_content_text_for_contains": LONG_RULE})
add("with_skill_safety_rule", summary="sum", head=[msg("system", content=RULE)], tail=[],
    io={"_content_text_for_contains": RULE})
add("with_skill_safety_rule", summary="sum", head=[msg("user", content=PREFIX + " a]")],
    tail=[msg("tool", content=PREFIX + " b]")],
    io={"_content_text_for_contains": [PREFIX + " a]", PREFIX + " b]"]})
add("with_skill_safety_rule", summary="sum", head=[attr(role="user", content=RULE)], tail=[],
    io={})
add("with_skill_safety_rule", summary=None, head=[], tail=[])
add("with_skill_safety_rule", summary="sum", head=None, tail=[])
add("with_skill_safety_rule", summary="sum", head=[msg("user", content=PREFIX + " a]")], tail=[],
    io={"_content_text_for_contains": None})  # RULE in None -> TypeError
add("with_skill_safety_rule", summary="sum", head=[msg("user", content=PREFIX + " a]")], tail=[],
    io={})  # fail-closed
add("with_skill_safety_rule", summary="sum", head=[msg("user", content=PREFIX + " a]")], tail=[],
    io={"_content_text_for_contains": raises("SystemExit")})
add("with_skill_safety_rule", summary="sum", head=["plain"], tail=[],
    io={})  # non-dict row skipped, no seam; no prune -> unchanged

# ── unruled (standalone nested closure) ──
add("unruled", rows=[msg("user", content=LONG_RULE)])
add("unruled", rows=[msg("system", content=RULE)])
add("unruled", rows=[msg("user", content="plain")])
add("unruled", rows=[msg("user", content=RULE), msg("tool", content="x" + SUFFIX)])
add("unruled", rows=[attr(role="user", content=RULE)])
add("unruled", rows=None)
add("unruled", rows=[])
add("unruled", rows=[msg("user", content=SUFFIX)])  # content == suffix -> replace -> ""
add("unruled", rows=[5])

# ── without_skill_safety_rule ──
add("without_skill_safety_rule", content="body" + SUFFIX)
add("without_skill_safety_rule", content="plain")
add("without_skill_safety_rule", content=SUFFIX)
add("without_skill_safety_rule", content="a" + SUFFIX + "b" + SUFFIX)
add("without_skill_safety_rule", content=None)
add("without_skill_safety_rule", content=5)
add("without_skill_safety_rule", content=[])
add("without_skill_safety_rule", content="")

# ── elide_keeping_skill_safety_rule ──
add("elide_keeping_skill_safety_rule", content="plain", limit=10, io={"elide": "PLAIN"})
add("elide_keeping_skill_safety_rule", content="body" + SUFFIX, limit=400,
    io={"elide": "BODY"})
add("elide_keeping_skill_safety_rule", content="body" + SUFFIX, limit=400,
    io={"elide": 5})  # + int -> TypeError
add("elide_keeping_skill_safety_rule", content=None, limit=10)
add("elide_keeping_skill_safety_rule", content="plain", limit="x", io={"elide": "P"})
add("elide_keeping_skill_safety_rule", content="body" + SUFFIX, limit="x")
add("elide_keeping_skill_safety_rule", content="plain", limit=10)  # fail-closed
add("elide_keeping_skill_safety_rule", content="body" + SUFFIX, limit=400,
    io={"elide": raises("GeneratorExit")})
add("elide_keeping_skill_safety_rule", content="plain", limit=10, io={"elide": None})
add("elide_keeping_skill_safety_rule", content=["x"], limit=10, io={"elide": "L"})
add("elide_keeping_skill_safety_rule", content="body" + SUFFIX, limit=400.0, io={"elide": "B"})

# ── with_pruned_row_skill_safety_rule ──
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=LONG)],
    pruned=[msg("tool", content=PREFIX + " a]")], io={"_content_text_for_contains": PREFIX + " a]"})
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=LONG)],
    pruned=[msg("tool", content=RULE)], io={"_content_text_for_contains": RULE})
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=LONG)],
    pruned=[msg("tool", content="plain")], io={"_content_text_for_contains": "plain"})
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=PREFIX + " a]")],
    pruned=[msg("tool", content=PREFIX + " a]")], io={"_content_text_for_contains": PREFIX + " a]"})
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content="x"), msg("tool", content=LONG)],
    pruned=[msg("tool", content="x"), msg("tool", content=PREFIX + " b]")],
    io={"_content_text_for_contains": ["x", PREFIX + " b]"]})
add("with_pruned_row_skill_safety_rule", original=None,
    pruned=[msg("tool", content=PREFIX + " a]")], io={"_content_text_for_contains": PREFIX + " a]"})
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=LONG)], pruned=None)
add("with_pruned_row_skill_safety_rule", original=[5],
    pruned=[msg("tool", content=PREFIX + " a]")], io={"_content_text_for_contains": PREFIX + " a]"})
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=LONG)],
    pruned=[msg("tool", content=PREFIX + " a]")], io={})  # fail-closed
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=LONG)],
    pruned=[msg("tool", content=PREFIX + " a]")],
    io={"_content_text_for_contains": raises("KeyboardInterrupt")})
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=LONG)],
    pruned=[attr(role="tool", content=PREFIX + " a]")], io={})
add("with_pruned_row_skill_safety_rule", original=[], pruned=[], io={})
add("with_pruned_row_skill_safety_rule", original=[msg("tool", content=LONG)],
    pruned=[msg("tool", content=PREFIX + " a]")], io={"_content_text_for_contains": 5})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-func count check (>=5 each)
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print("  %-40s %d%s" % (fn, counts[fn], flag))
    bad = [fn for fn, n in counts.items() if n < 5]
    if bad:
        raise SystemExit("FUNCS WITH <5 CASES: %s" % bad)


if __name__ == "__main__":
    main()

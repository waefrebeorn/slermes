#!/usr/bin/env python3
"""Generate tests/fixtures/context_budget_flow.jsonl — the scripted-seam case
corpus for the context_budget_flow FLOW + leaf port (14 funcs, >=5 cases each).
Real defining module: agent/context_compressor_window.py (+ leaves). ASCII
scope; integral floats == ints. Covers normal paths, branch edges, slice
clamping, in-place demote mutation, identity (is None) vs truthiness, real
scripted exceptions (incl. the BaseException trio), and fail-closed missing /
exhausted scripts. Run: python tests/gen_context_budget_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "context_budget_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def msg(role, **kw):
    d = {"role": role}
    d.update(kw)
    return d


def raises(kind, message=""):
    return {"__raise__": kind, "message": message}


def sc(ret):
    return {"__script__": True, "ret": ret}


def attr(**kw):
    return {"__attr__": kw}


def tc(id_, name, arguments="{}"):
    return {"id": id_, "function": {"name": name, "arguments": arguments}}


def tool(id_, content):
    return msg("tool", tool_call_id=id_, content=content)


def asst(*calls):
    return msg("assistant", tool_calls=list(calls))


# ── _restart_probe_extra ──
add("_restart_probe_extra", io={"import_const": 4})
add("_restart_probe_extra", io={"import_const": 0})
add("_restart_probe_extra", io={"import_const": sc(7)})
add("_restart_probe_extra", io={})                                   # fail-closed
add("_restart_probe_extra", io={"import_const": raises("SystemExit", "bye")})
add("_restart_probe_extra", io={"import_const": [4, 5]})             # queue, one call

# ── summary_view ──
P1 = [tool("t1", "pruned"), msg("user", content="u")]
O1 = [tool("t1", "original"), msg("user", content="u")]
add("summary_view", plan=P1, originals=O1, start=0, end=2,
    io={})                                                           # no calls -> original restored
add("summary_view", plan=P1, originals=[tool("t1", "orig"), msg("user", content="u")],
    start=0, end=2, io={})
# clarify keeps pruned form: needs an assistant call naming t1 -> clarify
P2 = [asst(tc("t1", "clarify")), tool("t1", "pruned"), msg("user", content="u")]
O2 = [asst(tc("t1", "clarify")), tool("t1", "original"), msg("user", content="u")]
add("summary_view", plan=P2, originals=O2, start=0, end=3, io={})
P3 = [asst(tc("t1", "read_file")), tool("t1", "pruned")]
O3 = [asst(tc("t1", "read_file")), tool("t1", "original")]
add("summary_view", plan=P3, originals=O3, start=1, end=2, io={})    # window slice middle
add("summary_view", plan=P3, originals=O3, start=-1, end=2, io={})   # negative start
add("summary_view", plan=P3, originals=O3[:1], start=0, end=2, io={})  # len mismatch -> plan
add("summary_view", plan=[tool("tx", "p")], originals=[tool("tx", "o")], start=0, end=1, io={})  # unknown id
add("summary_view", plan=[5], originals=[5], start=0, end=1, io={})  # non-dict original -> .get raise
add("summary_view", plan=[tool(None, "p")], originals=[tool(None, "o")], start=0, end=1, io={})  # null id -> ""

# ── _compress_window ──
MSGS = [msg("system"), msg("user", content="a"), msg("assistant", content="b"), msg("user", content="c")]
CW_BASE = {"_protect_head_size": 1, "_align_boundary_forward": 1,
           "_find_tail_cut_by_tokens": 4, "_find_last_user_message_idx": 3,
           "_is_context_summary_message": [False, False, False, False]}
add("_compress_window", messages=MSGS, state={}, io=dict(CW_BASE))   # end != latest
add("_compress_window", messages=MSGS, state={},
    io={**CW_BASE, "_find_tail_cut_by_tokens": 3})                   # bridge assistant, floor 2 -> end 2
add("_compress_window", messages=[msg("system"), msg("user", content="a"), tool("t1", "x"), msg("user", content="c")],
    state={}, io={**CW_BASE, "_find_tail_cut_by_tokens": 3, "_align_boundary_backward": 2})  # bridge tool
add("_compress_window", messages=[msg("system"), msg("user", content="a"), msg("user", content="b"), msg("user", content="c")],
    state={}, io={**CW_BASE, "_find_tail_cut_by_tokens": 3})         # bridge role user -> -1
add("_compress_window", messages=MSGS, state={},
    io={**CW_BASE, "_find_tail_cut_by_tokens": 0, "_find_last_user_message_idx": 0})  # latest 0 -> bridge -1
add("_compress_window", messages=[msg("system"), 5, msg("user", content="c")], state={},
    io={**CW_BASE, "_find_tail_cut_by_tokens": 2, "_find_last_user_message_idx": 2})  # bridge non-dict
add("_compress_window", messages=MSGS, state={},
    io={**CW_BASE, "_find_tail_cut_by_tokens": 9, "_find_last_user_message_idx": 9})  # bridge idx oob
add("_compress_window", messages=MSGS, state={}, io={"_align_boundary_forward": 1})   # fail-closed head
add("_compress_window", messages=MSGS, state={},
    io={**CW_BASE, "_find_tail_cut_by_tokens": raises("KeyboardInterrupt")})
# floor raises past handoff group: newest summary at idx 1, all handoff, raised 3
add("_compress_window", messages=[msg("system"), msg("user", content="handoff"), tool("t1", "x"), msg("user", content="c")],
    state={}, io={"_protect_head_size": 1, "_align_boundary_forward": [1, 3],
                  "_find_tail_cut_by_tokens": 3, "_find_last_user_message_idx": 3,
                  "_is_context_summary_message": [False, True, True, True],
                  "_find_last_user_message_idx": 3})

# ── _plan_compaction_window ──
PCW_ST = {"protect_last_n": 2, "tail_token_budget": 500}
PCW_IO = {"_prune_old_tool_results": sc([MSGS, 1]), "_drop_blank_echoes": sc(MSGS), **CW_BASE}
add("_plan_compaction_window", messages=MSGS, state=PCW_ST, io=dict(PCW_IO))
add("_plan_compaction_window", messages=MSGS, state={"tail_token_budget": 500}, io=dict(PCW_IO))  # protect_last_n absent
add("_plan_compaction_window", messages=MSGS, state={"protect_last_n": 2}, io=dict(PCW_IO))        # tail_token_budget absent
add("_plan_compaction_window", messages=MSGS, state=PCW_ST, io={"_drop_blank_echoes": sc(MSGS), **CW_BASE})  # fail-closed prune
add("_plan_compaction_window", messages=MSGS, state=PCW_ST,
    io={**PCW_IO, "_prune_old_tool_results": sc([MSGS])})            # unpack wrong length
add("_plan_compaction_window", messages=MSGS, state=PCW_ST,
    io={**PCW_IO, "_drop_blank_echoes": raises("ValueError", "bad")})
add("_plan_compaction_window", messages=MSGS, state=PCW_ST,
    io={**PCW_IO, "_prune_old_tool_results": sc([MSGS, 0]), "_find_tail_cut_by_tokens": 3})  # composed bridge

# ── _commit_window ──
CW_ST = {"_session_id": "sess1", "tail_mode": "lean"}
OW_IO = {"import_const": 200, "_backend_reads_host_files": False}
add("_commit_window", originals=O1, plan=P1, pruned_count=0, start=2, end=2, state=CW_ST,
    io={"_demote_stale_tail_tools": sc(P1)})                            # lean demote only
add("_commit_window", originals=O1, plan=P1, pruned_count=2, start=2, end=2, state=CW_ST,
    io={"archive_compaction_drops": sc([P1, ["w"], 1]), "_demote_stale_tail_tools": sc(P1)})  # kept subtract
add("_commit_window", originals=O1, plan=P1, pruned_count=0, start=0, end=1, state=CW_ST,
    io={**OW_IO, "_demote_stale_tail_tools": sc(P1)})                   # composed offload (backend false)
add("_commit_window", originals=O1, plan=P1, pruned_count=0, start=0, end=1, state=CW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True,
        "_demote_tool_result_at": {"ret": True, "row": tool("t1", "stub")},
        "archive_pruned_originals": sc([P1, ["w"], 1]),
        "logger.warning": None,
        "_demote_stale_tail_tools": sc(P1)})                            # failed=1 -> logger.warning
add("_commit_window", originals=O1, plan=P1, pruned_count=0, start=2, end=2,
    state={"_session_id": "s", "tail_mode": "full"}, io={})         # full -> no demote
add("_commit_window", originals=O1, plan=P1, pruned_count=0, start=2, end=2,
    state={"_session_id": "s"}, io={"_demote_stale_tail_tools": sc(P1)})  # tail_mode absent -> lean
add("_commit_window", originals=O1, plan=P1, pruned_count=1, start=2, end=2, state={},
    io={"archive_compaction_drops": sc([P1, [], 0]), "_demote_stale_tail_tools": sc(P1)})  # sid absent -> ""
add("_commit_window", originals=O1, plan=P1, pruned_count=1, start=2, end=2, state=CW_ST,
    io={"archive_compaction_drops": raises("OSError", "disk"), "_demote_stale_tail_tools": sc(P1)})

# ── _assemble_with_archive_list ──
AW_MSGS = [tool("t1", "see cache/pruned/a.json"), msg("user", content="u")]
AW_IO = {"pruned_archive_refs": sc(["cache/pruned/a.json"]), "archive_list_for_summary": "",
         "_assemble_compressed": sc(AW_MSGS)}
add("_assemble_with_archive_list", messages=AW_MSGS, start=0, end=1, scan=attr(tail_start=1),
    summary="SUMMARY", state={}, io=dict(AW_IO))
add("_assemble_with_archive_list", messages=AW_MSGS, start=0, end=1, scan=attr(tail_start=1),
    summary="SUMMARY", state={}, io={**AW_IO, "archive_list_for_summary": "\n\n- ref"})
add("_assemble_with_archive_list", messages=AW_MSGS, start=0, end=1, scan=attr(tail_start=5),
    summary="S", state={}, io=dict(AW_IO))                          # max(end, tail_start)
add("_assemble_with_archive_list", messages=AW_MSGS, start=0, end=1, scan=attr(),
    summary="S", state={}, io=dict(AW_IO))                          # tail_start absent
add("_assemble_with_archive_list", messages=AW_MSGS, start=0, end=1, scan=attr(tail_start=1),
    summary="S", state={}, io={"archive_list_for_summary": "", "_assemble_compressed": sc(AW_MSGS)})  # fail-closed refs
add("_assemble_with_archive_list", messages=AW_MSGS, start=0, end=1, scan=attr(tail_start=1),
    summary="S", state={}, io={**AW_IO, "archive_list_for_summary": raises("GeneratorExit")})
add("_assemble_with_archive_list", messages=AW_MSGS, start=0, end=1, scan=attr(tail_start=1),
    summary=5, state={}, io=dict(AW_IO))                            # summary non-str concat
add("_assemble_with_archive_list", messages=AW_MSGS, start=-1, end=1, scan=attr(tail_start=1),
    summary="S", state={}, io=dict(AW_IO))                          # negative start slice

# ── _offload_window_results ──
OW_PLAN = [asst(tc("t1", "read_file")), tool("t1", "a long result body here")]
OW_ST = {"_session_id": "sess1"}
add("_offload_window_results", plan=OW_PLAN, start=0, end=2, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": False})   # early return
add("_offload_window_results", plan=OW_PLAN, start=0, end=2, state=OW_ST,
    io={"_backend_reads_host_files": False})                        # import_const fail-closed
add("_offload_window_results", plan=OW_PLAN, start=1, end=2, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True,
        "_demote_tool_result_at": {"ret": True, "row": tool("t1", "stub")},
        "archive_pruned_originals": sc([OW_PLAN, ["w"], 0])})       # demote seam true+row
add("_offload_window_results", plan=[tool("t1", "short")], start=0, end=1, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True,
        "_demote_tool_result_at": False, "has_archive_pointer": False,
        "drop_stale_api_content": None,
        "archive_pruned_originals": sc([[tool("t1", "[Tool output archived]")], ["w"], 0])})  # elif str
add("_offload_window_results", plan=[tool("t1", "short")], start=0, end=1, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True,
        "_demote_tool_result_at": False, "has_archive_pointer": True})  # pointer -> no demote
add("_offload_window_results", plan=[tool("t1", ["block"])], start=0, end=1, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True,
        "_demote_tool_result_at": False, "drop_stale_api_content": None,
        "archive_pruned_originals": sc([[tool("t1", "[Tool output archived]")], [], 0])})  # content list
add("_offload_window_results", plan=[msg("user", content="x")], start=0, end=1, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True, "_demote_tool_result_at": False})  # non-tool
add("_offload_window_results", plan=OW_PLAN, start=1, end=2, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True,
        "_demote_tool_result_at": {"ret": True, "row": tool("t1", "stub")},
        "archive_pruned_originals": sc([OW_PLAN, ["w"], 1])})       # kept 1 -> stubbed 0
add("_offload_window_results", plan=[5], start=0, end=1, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True})    # non-dict -> calls_by_id raise
add("_offload_window_results", plan=OW_PLAN, start=2, end=1, state=OW_ST,
    io={"import_const": 200, "_backend_reads_host_files": True})    # start>end empty loop

# ── _take_prepared_summary ──
TP_ST = {"prepared_compaction": {"_taken": "x"}, "_session_id": "sess1"}
add("_take_prepared_summary", messages=MSGS, compress_start=1, fresh=False, provider_context=False,
    state={"prepared_compaction": None, "_session_id": "s"}, io={})  # null -> None
add("_take_prepared_summary", messages=MSGS, compress_start=1, state={"_session_id": "s"}, io={})  # absent -> None
add("_take_prepared_summary", messages=MSGS, compress_start=1, fresh=False, provider_context=False,
    state=TP_ST, io={"take": {"summary": "prepared"}})              # eligible true
add("_take_prepared_summary", messages=MSGS, compress_start=1, fresh=False, provider_context=True,
    state=TP_ST, io={"stop_for_provider_context": None, "take": None})  # stop + eligible false
add("_take_prepared_summary", messages=MSGS, compress_start=1, fresh=True, provider_context=False,
    state=TP_ST, io={"take": None})                                 # fresh -> eligible false
add("_take_prepared_summary", messages=MSGS, compress_start=1,
    state={"prepared_compaction": {"_taken": "x"}}, io={"take": None})  # _session_id absent
add("_take_prepared_summary", messages=MSGS, compress_start=1, state=TP_ST,
    io={"take": raises("CancelledError")})                          # BaseException
add("_take_prepared_summary", messages=MSGS, compress_start=1, provider_context=True,
    state={"prepared_compaction": 5, "_session_id": "s"}, io={})    # non-carrier stop
add("_take_prepared_summary", messages=MSGS, compress_start=1, provider_context=False,
    state={"prepared_compaction": 5, "_session_id": "s"}, io={})    # non-carrier take

# ── _adopt_prepared_summary ──
add("_adopt_prepared_summary", prepared={"summary": "handoff text"}, state={}, io={"adopt": None})
add("_adopt_prepared_summary", prepared={"body": "b"}, state={}, io={"adopt": None})  # summary absent
add("_adopt_prepared_summary", prepared={"summary": "s"}, state={}, io={})            # fail-closed adopt
add("_adopt_prepared_summary", prepared={"summary": "s"}, state={}, io={"adopt": raises("KeyboardInterrupt")})
add("_adopt_prepared_summary", prepared={"summary": "s"}, state={"_session_id": "x", "tail_mode": "lean"},
    io={"adopt": None})                                             # compressor projection
add("_adopt_prepared_summary", prepared={"summary": "s", "body": "b", "telemetry": {"k": 1}},
    state={}, io={"adopt": None})                                   # extra fields

# ── tail_cut_progress_floor ──
TF_MSGS = [msg("system"), msg("user", content="a"), msg("assistant", content="b"), msg("user", content="c")]
add("tail_cut_progress_floor", messages=TF_MSGS, head_end=1, cut_idx=3, state={},
    io={"_is_context_summary_message": [False, False, False]})      # no summary -> floor 2
add("tail_cut_progress_floor", messages=TF_MSGS, head_end=1, cut_idx=3, state={},
    io={"_is_context_summary_message": [False, True, True, True],
        "_align_boundary_forward": 3, "_find_last_user_message_idx": 3})  # raised 3, last_user 3 -> floor
add("tail_cut_progress_floor", messages=TF_MSGS, head_end=1, cut_idx=3, state={},
    io={"_is_context_summary_message": [False, True, True, True],
        "_align_boundary_forward": 9, "_find_last_user_message_idx": 3})  # raised >= len -> floor
add("tail_cut_progress_floor", messages=TF_MSGS, head_end=1, cut_idx=3, state={},
    io={"_is_context_summary_message": [False, True, True, True],
        "_align_boundary_forward": 3, "_find_last_user_message_idx": 1})  # last_user < raised -> raised
add("tail_cut_progress_floor", messages=TF_MSGS, head_end=1, cut_idx=3, state={},
    io={"_is_context_summary_message": [False, True, False]})       # not all handoff -> floor
add("tail_cut_progress_floor", messages=TF_MSGS, head_end=1, cut_idx=3, state={},
    io={"_is_context_summary_message": raises("RuntimeError", "x")})
add("tail_cut_progress_floor", messages=TF_MSGS, head_end=9, cut_idx=3, state={}, io={})  # head_end>len -> floor
add("tail_cut_progress_floor", messages=TF_MSGS, head_end=1, cut_idx=-1, state={},
    io={"_is_context_summary_message": [False, True, True, True],
        "_align_boundary_forward": 3, "_find_last_user_message_idx": 3})  # max(cut_idx, newest+1)

# ── _tool_calls_by_id ──
add("_tool_calls_by_id", messages=[asst(tc("t1", "clarify")), tool("t1", "r")], io={})
add("_tool_calls_by_id", messages=[msg("user", content="hi")], io={})
add("_tool_calls_by_id", messages=[msg("assistant", tool_calls=None)], io={})
add("_tool_calls_by_id", messages=[asst(tc("t1", "a"), tc("t1", "b"))], io={})  # dup id last wins
add("_tool_calls_by_id", messages=[msg("assistant", tool_calls=[5])], io={})    # tc non-dict
add("_tool_calls_by_id", messages=[msg("assistant", tool_calls=[{"id": "t1", "function": None}])], io={})  # fn null
add("_tool_calls_by_id", messages=None, io={})                                  # not iterable
add("_tool_calls_by_id", messages=[5], io={})                                   # non-dict msg .get
add("_tool_calls_by_id", messages=[msg("assistant", tool_calls=[{"id": 0, "function": {"name": "n"}}])], io={})  # falsy id

# ── _tc_get ──
add("_tc_get", obj={"id": "t1"}, key="id", default="", io={})
add("_tc_get", obj={"id": "t1"}, key="missing", default="def", io={})
add("_tc_get", obj={"id": None}, key="id", default="def", io={})    # present null -> null
add("_tc_get", obj=5, key="id", default="def", io={})               # scalar -> default
add("_tc_get", obj=attr(id="t1"), key="id", default="", io={})      # attr double present
add("_tc_get", obj=attr(id="t1"), key="missing", default="def", io={})  # attr double absent
add("_tc_get", obj=None, key="id", default="def", io={})            # null -> default
add("_tc_get", obj={"id": "t1"}, key="id", io={})                   # default omitted -> ""

# ── _rewritten ──
add("_rewritten", msg=tool("t1", "old"), content="new", io={"drop_stale_api_content": None})
add("_rewritten", msg={"role": "tool", "content": "old", "api_content": "stale"}, content="new",
    io={"drop_stale_api_content": None})                            # api_content popped
add("_rewritten", msg={"a": 1, "content": "old", "b": 2}, content="new",
    io={"drop_stale_api_content": None})                            # content position kept
add("_rewritten", msg=tool("t1", "old"), content="new", io={"drop_stale_api_content": raises("RuntimeError", "x")})
add("_rewritten", msg=5, content="new", io={})                      # non-mapping
add("_rewritten", msg=None, content="new", io={})                   # NoneType mapping
add("_rewritten", msg=tool("t1", "old"), content=None, io={"drop_stale_api_content": None})  # content null

# ── archive_refs_in_text ──
add("archive_refs_in_text", texts=["see cache/pruned/abc_12.json for details"], io={})
add("archive_refs_in_text", texts=["spillover/x.txt and cache/spillover/y.json"], io={})
add("archive_refs_in_text", texts=["spillover\\win.txt backslash"], io={})
add("archive_refs_in_text", texts=["no refs here", 5, None, ["cache/pruned/z.txt"]], io={})  # non-str skipped
add("archive_refs_in_text", texts=["cache/pruned/a cache/pruned/a", "cache/pruned/a"], io={})  # dedup
add("archive_refs_in_text", texts=["nothing"], io={})
add("archive_refs_in_text", texts=[], io={})
add("archive_refs_in_text", texts=["cache/pruned/a.b-c_d", "xspillover/abc"], io={})  # name class + prefix
add("archive_refs_in_text", texts=["spillover/ cache/pruned/b"], io={})  # spillover no name


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for rec in CASES:
            f.write(json.dumps(rec, ensure_ascii=True) + "\n")
    # per-func coverage check (>=5 each)
    counts = {}
    for rec in CASES:
        counts[rec["func"]] = counts.get(rec["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for fn in sorted(counts):
        flag = "" if counts[fn] >= 5 else "  (<5!)"
        print("  %-32s %d%s" % (fn, counts[fn], flag))
    assert all(c >= 5 for c in counts.values()), "a func has <5 cases"
    assert len(counts) == 14, "expected 14 funcs, got %d" % len(counts)


if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""Generate tests/fixtures/turn_exit_flow.jsonl — scripted-seam FLOW cases
for the turn_exit_flow port (the UNCLAIMED turn-exit surface consuming
hermes_cli/turn_exit.py, whose own two defs are claimed by sk2 and seamed).

Every case: {"func": <harness name>, "in": {... inputs ..., "seams": {name: script}}}
A seam script is a single result, or a JSON array = one result PER CALL (last
repeats). A result that is ITSELF a list must be wrapped one level deeper
([[...]]) so the array is not read as a per-call sequence. {"__raise__": cls,
"msg": m} raises that exception class in the live drive and is delivered as a
raise descriptor to the C universal seam. A MISSING seam key fail-closes
(RuntimeError) on BOTH sides.

Domains (documented in the port header): ASCII strings, POSIX paths,
integral ints; reasons sets are ordered arrays; strip_ansi scripts are str;
get_task scripts are null or a dict; extract_image_refs / build_native_
content_parts scripts are 2-element arrays (the live tuple unpack).
Run: python tests/gen_turn_exit_flow_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "turn_exit_flow.jsonl")

CASES = []


def add(func, inp, seams):
    inp = dict(inp)
    inp["seams"] = seams
    CASES.append({"func": func, "in": inp})


def rs(cls, msg=""):
    return {"__raise__": cls, "msg": msg}


# ── _int_or ──
add("int_or", {"value": "42", "default": 0}, {})
add("int_or", {"value": "  -17  ", "default": 0}, {})
add("int_or", {"value": "1_000", "default": 0}, {})
add("int_or", {"value": "5\x1c", "default": 9}, {})          # int() does NOT strip \x1c
add("int_or", {"value": 5.9, "default": 0}, {})
add("int_or", {"value": -5.9, "default": 0}, {})
add("int_or", {"value": True, "default": 0}, {})
add("int_or", {"value": None, "default": 7}, {})
add("int_or", {"value": [1], "default": None}, {})
add("int_or", {"value": "abc", "default": 3}, {})
add("int_or", {"value": "_1", "default": 0}, {})
add("int_or", {"value": "1_", "default": 0}, {})
add("int_or", {"value": "", "default": 5}, {})

# ── _single_query_exit_code ──
TRANS = ["rate_limit", "upstream_rate_limit", "billing", "overloaded", "server_error", "timeout"]
TERM = ["auth", "auth_permanent", "model_not_found", "ssl_cert_verification", "upstream_blocked"]


def sq_seams(task, code, **over):
    s = {
        "import__TERMINAL_PROVIDER_REASONS": [TERM],   # wrapped list result
        "import__TRANSIENT_PROVIDER_REASONS": [TRANS],
        "import_kanban_task_id": None,
        "kanban_task_id": task,
        "import_turn_exit_code": None,
        "turn_exit_code": code,
    }
    s.update(over)
    return s


add("single_query_exit_code", {"result": {"completed": True}}, sq_seams(None, 0))
add("single_query_exit_code", {"result": {"failed": True, "failure_reason": "rate_limit"}},
    sq_seams("t1", 75))
add("single_query_exit_code", {"result": None, "credentials_rate_limited": True},
    sq_seams("t1", 75))
add("single_query_exit_code", {"result": None, "credentials_terminal": True},
    sq_seams("t1", 78))
add("single_query_exit_code", {"result": {"interrupted": True}}, sq_seams(None, 130))
add("single_query_exit_code", {"result": {"failed": True}},
    sq_seams("t1", rs("RuntimeError", "boom")))
add("single_query_exit_code", {"result": {"completed": True}},
    sq_seams(None, 0, kanban_task_id=rs("ValueError", "bad task")))
add("single_query_exit_code", {"result": {"completed": True}},
    {"import__TERMINAL_PROVIDER_REASONS": [TERM],
     "import__TRANSIENT_PROVIDER_REASONS": [TRANS],
     "import_kanban_task_id": None, "kanban_task_id": None,
     "import_turn_exit_code": None})  # fail-closed: turn_exit_code unscripted
add("single_query_exit_code", {"result": {"completed": True}},
    sq_seams(None, 0, import_kanban_task_id=rs("ImportError", "no module")))
add("single_query_exit_code", {"result": {"completed": True}},
    {"import__TERMINAL_PROVIDER_REASONS": None,
     "import__TRANSIENT_PROVIDER_REASONS": None,
     "import_kanban_task_id": None, "kanban_task_id": None,
     "import_turn_exit_code": None, "turn_exit_code": 1})

# ── _sync_cli_session_id_from_agent ──
add("sync_cli_session_id", {"cli": {"session_id": "s1", "agent": {"session_id": "s2"}}}, {})
add("sync_cli_session_id", {"cli": {"session_id": "s1", "agent": {"session_id": "s1"}}}, {})
add("sync_cli_session_id", {"cli": {"session_id": "s1", "agent": {"session_id": ""}}}, {})
add("sync_cli_session_id", {"cli": {"session_id": "s1", "agent": {}}}, {})
add("sync_cli_session_id", {"cli": {"session_id": "s1", "agent": None}}, {})
add("sync_cli_session_id", {"cli": {"session_id": "", "agent": {"session_id": "s2"}}}, {})

# ── _interrupt_agent_for_signal ──
def sig_seams(grace, **over):
    s = {"import__float_env": None, "request_hard_interrupt": None,
         "float_env": grace, "time_sleep": None}
    s.update(over)
    return s


add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15}, sig_seams(1.5))
add("interrupt_agent_for_signal", {"agent": None, "signum": 2}, sig_seams(1.5))
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15}, sig_seams(0))
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15}, sig_seams(-1))
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15},
    sig_seams(1.5, request_hard_interrupt=rs("RuntimeError", "interrupt fail")))
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15},
    sig_seams(1.5, request_hard_interrupt=rs("KeyboardInterrupt")))
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15},
    sig_seams(1.5, float_env=rs("ValueError", "bad env")))
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15},
    sig_seams(1.5, time_sleep=rs("SystemExit", "0")))
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15}, sig_seams(None))
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15},
    {"import__float_env": None, "request_hard_interrupt": None})  # fail-closed float_env
add("interrupt_agent_for_signal", {"agent": {"id": "a"}, "signum": 15},
    sig_seams(1.5, import__float_env=rs("ImportError", "no cli")))

# ── _configure_quiet_agent ──
add("configure_quiet_agent", {"agent": {}}, {})
add("configure_quiet_agent", {"agent": {"quiet_mode": False, "tool_progress_mode": "auto"}}, {})
add("configure_quiet_agent", {"agent": {"extra": "x", "stream_delta_callback": "cb"}}, {})
add("configure_quiet_agent", {"agent": {"tool_progress_mode": "off"}}, {})
add("configure_quiet_agent", {"agent": {"quiet_mode": True, "suppress_status_output": True,
                                        "tool_progress_mode": "off", "keep": 1}}, {})

# ── _run_kanban_goal_loop_chat ──
add("run_kanban_goal_loop_chat", {"cli": {"session_id": "s"}, "first_response": "done"},
    {"import__run_kanban_goal_loop_q": None, "run_kanban_goal_loop_q": None})
add("run_kanban_goal_loop_chat", {"cli": {"session_id": "s"}, "first_response": ""},
    {"import__run_kanban_goal_loop_q": None, "run_kanban_goal_loop_q": None})
add("run_kanban_goal_loop_chat", {"cli": {}, "first_response": "x"},
    {"import__run_kanban_goal_loop_q": None, "run_kanban_goal_loop_q": rs("RuntimeError", "loop")})
add("run_kanban_goal_loop_chat", {"cli": {}, "first_response": "x"},
    {"import__run_kanban_goal_loop_q": None, "run_kanban_goal_loop_q": rs("SystemExit", "0")})
add("run_kanban_goal_loop_chat", {"cli": {}, "first_response": "x"},
    {"import__run_kanban_goal_loop_q": None})  # fail-closed
add("run_kanban_goal_loop_chat", {"cli": {}, "first_response": "x"},
    {"import__run_kanban_goal_loop_q": rs("ImportError", "no cli")})
add("run_kanban_goal_loop_chat", {"cli": None, "first_response": "y"},
    {"import__run_kanban_goal_loop_q": None, "run_kanban_goal_loop_q": None})

# ── _collect_kanban_task_images ──
def col_seams(task, refs, **over):
    s = {"import_extract_image_refs": None, "import_connect_closing": None,
         "connect_closing": None, "connect_closing_enter": "conn1",
         "connect_closing_exit": None, "import_get_task": None,
         "get_task": task, "extract_image_refs": [refs]}   # wrapped tuple result
    s.update(over)
    return s


add("collect_kanban_task_images",
    {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "see a.png"}, [["a.png"], ["http://u"]]))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": ""}, "images": []}, {})
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "   "}, "images": []}, {})
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams(None, [[], []]))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({}, [[], []]))
add("collect_kanban_task_images",
    {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": ["a.png"]},
    col_seams({"body": "b"}, [["a.png", "b.png"], []]))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [["./x/./y.png"], []]))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [["a/./b.png", "a/./b.png"], []]))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [[], []], get_task=rs("RuntimeError", "db")))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [[], []], connect_closing_exit=rs("RuntimeError", "close")))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [[], []], extract_image_refs=rs("ValueError", "refs")))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [["only-one"]]))  # wrong arity unpack
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [["a.png"], None]))  # extend(None) TypeError
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    {"import_extract_image_refs": None, "import_connect_closing": None,
     "connect_closing": None, "connect_closing_enter": "conn1",
     "connect_closing_exit": None, "import_get_task": None})  # fail-closed get_task
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [[], []], import_extract_image_refs=rs("KeyboardInterrupt")))
add("collect_kanban_task_images", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "images": []},
    col_seams({"body": "b"}, [[], []], connect_closing_enter=rs("RuntimeError", "enter")))

# ── _route_single_query_images ──
def route_seams(mode, parts=None, **over):
    s = {"import_build_native_content_parts": None,
         "import_decide_image_input_mode": None, "import_load_config": None,
         "load_config": {"cfg": 1}, "decide_image_input_mode": mode,
         "preprocess_images_with_vision": "fallback-text"}
    if parts is not None:
        s["build_native_content_parts"] = [[parts, []]]   # wrapped tuple result
    s.update(over)
    return s


IMG_PARTS = [{"type": "image_url", "image_url": {"url": "http://u"}}]
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": [], "image_urls": []}, {})
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("text"))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": [], "image_urls": ["u"]},
    route_seams("text"))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("native", IMG_PARTS))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("native", [{"type": "text", "text": "hi"}]))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("native", build_native_content_parts=rs("RuntimeError", "build")))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("native", [{"type": "text"}],
                preprocess_images_with_vision=[rs("RuntimeError", "first"), "second"]))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("text", preprocess_images_with_vision=rs("RuntimeError", "early")))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("native", [{"type": "text"}],
                preprocess_images_with_vision=rs("KeyboardInterrupt")))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("text", decide_image_input_mode=rs("RuntimeError", "decide")))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("text", load_config=rs("ValueError", "cfg")))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("text", import_build_native_content_parts=rs("ImportError", "no img")))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    {"import_build_native_content_parts": None, "import_decide_image_input_mode": None,
     "import_load_config": None, "load_config": {"cfg": 1},
     "preprocess_images_with_vision": "fb"})  # fail-closed decide
add("route_single_query_images",
    {"cli": {"provider": " openai ", "model": " gpt ", "requested_provider": " rp "},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("text"))
add("route_single_query_images",
    {"cli": {"provider": "openai", "model": "gpt", "requested_provider": ""},
     "query": "q", "effective_query": "eq", "images": ["a.png"], "image_urls": []},
    route_seams("native", [5]))  # parts row non-dict -> AttributeError caught

# ── _pending_preview ──
add("pending_preview", {"text": "hello world"}, {"import_strip_ansi": None, "strip_ansi": "hello world"})
add("pending_preview", {"text": "a\tb\nc"}, {"import_strip_ansi": None, "strip_ansi": "a\tb\nc"})
add("pending_preview", {"text": "  multiple   spaces  "},
    {"import_strip_ansi": None, "strip_ansi": "  multiple   spaces  "})
add("pending_preview", {"text": None}, {"import_strip_ansi": None})
add("pending_preview", {"text": 42}, {"import_strip_ansi": None})
add("pending_preview", {"text": "x" * 100},
    {"import_strip_ansi": None, "strip_ansi": "x" * 100})
add("pending_preview", {"text": "y" * 80}, {"import_strip_ansi": None, "strip_ansi": "y" * 80})
add("pending_preview", {"text": "a\x1cb"}, {"import_strip_ansi": None, "strip_ansi": "a\x1cb"})
add("pending_preview", {"text": "z"}, {"import_strip_ansi": None, "strip_ansi": rs("RuntimeError", "strip")})
add("pending_preview", {"text": "z"}, {"import_strip_ansi": None})  # fail-closed strip_ansi

# ── _quoted ──
add("quoted", {"row": {"text": "hi", "admission_id": "a1"}, "fallback": "lost turn"},
    {"import_strip_ansi": None, "strip_ansi": "hi"})
add("quoted", {"row": {"text": "", "admission_id": "a1"}, "fallback": "lost turn"},
    {"import_strip_ansi": None, "strip_ansi": ""})
add("quoted", {"row": {"admission_id": "a2"}, "fallback": "queued message"},
    {"import_strip_ansi": None})
add("quoted", {"row": {"text": ""}, "fallback": "lost turn"},
    {"import_strip_ansi": None, "strip_ansi": ""})  # KeyError admission_id
add("quoted", {"row": {"text": "   ", "admission_id": "a3"}, "fallback": "lost turn"},
    {"import_strip_ansi": None, "strip_ansi": "   "})
add("quoted", {"row": {"text": "x", "admission_id": "a4"}, "fallback": "lost turn"},
    {"import_strip_ansi": None, "strip_ansi": rs("ValueError", "strip")})
add("quoted", {"row": {"text": "x", "admission_id": "a5"}, "fallback": "lost turn"},
    {"import_strip_ansi": None})  # fail-closed strip_ansi

# ── pending_lines ──
add("pending_lines", {"pending": []}, {})
add("pending_lines", {"pending": [{"admission_id": "a1", "status": "unknown", "text": "help me"}],
                      "interactive": True},
    {"import_strip_ansi": None, "strip_ansi": ["help me", "help me"]})
add("pending_lines", {"pending": [{"admission_id": "a1", "status": "unknown"}],
                      "interactive": True},
    {"import_strip_ansi": None})
add("pending_lines", {"pending": [{"admission_id": "a1", "status": "unknown", "text": "t"}],
                      "interactive": False},
    {"import_strip_ansi": None, "strip_ansi": ["t", "t"]})
add("pending_lines", {"pending": [{"admission_id": "a1", "status": "unknown", "text": "t1"},
                                  {"admission_id": "a2", "status": "unknown", "text": "t2"}],
                      "interactive": True},
    {"import_strip_ansi": None, "strip_ansi": ["t1", "t2"]})
add("pending_lines", {"pending": [{"admission_id": "q1", "status": "queued", "text": "qm"}]},
    {"import_strip_ansi": None, "strip_ansi": ["qm"]})
add("pending_lines", {"pending": [{"admission_id": "a1", "status": "unknown"},
                                  {"admission_id": "q1", "status": "queued", "text": "m1"},
                                  {"admission_id": "q2", "status": "queued", "text": "m2"}]},
    {"import_strip_ansi": None, "strip_ansi": ["m1", "m2"]})
add("pending_lines", {"pending": [{"admission_id": "a1", "status": "unknown"},
                                  {"admission_id": "a2", "status": "unknown"},
                                  {"admission_id": "q1", "status": "queued", "text": "m1"},
                                  {"admission_id": "q2", "status": "queued", "text": "m2"},
                                  {"admission_id": "q3", "status": "queued", "text": "m3"}]},
    {"import_strip_ansi": None, "strip_ansi": ["m1", "m2", "m3"]})
add("pending_lines", {"pending": [{"admission_id": "a1"}]}, {})  # KeyError status
add("pending_lines", {"pending": [{"admission_id": "a1", "status": "unknown", "text": "t"}]},
    {"import_strip_ansi": None})  # fail-closed strip_ansi
add("pending_lines", {"pending": [{"admission_id": "q1", "status": "queued", "text": "m1"},
                                  {"admission_id": "a1", "status": "unknown", "text": "lost"}],
                      "interactive": True},
    {"import_strip_ansi": None, "strip_ansi": ["lost", "lost", "m1"]})

# ── lost_admission ──
add("lost_admission", {"snapshot": {"pending": [{"admission_id": "a1", "status": "unknown"}]},
                       "admission_id": "a1"}, {})
add("lost_admission", {"snapshot": {"pending": [{"admission_id": "a1", "status": "unknown"}]},
                       "admission_id": "a2"}, {})
add("lost_admission", {"snapshot": {"pending": [{"admission_id": "a1", "status": "queued"}]},
                       "admission_id": "a1"}, {})
add("lost_admission", {"snapshot": {"pending": []}, "admission_id": "a1"}, {})
add("lost_admission", {"snapshot": {}, "admission_id": "a1"}, {})
add("lost_admission", {"snapshot": None, "admission_id": "a1"}, {})
add("lost_admission", {"snapshot": {"pending": [{"status": "unknown"}]}, "admission_id": "a1"}, {})
add("lost_admission", {"snapshot": {"pending": [{"admission_id": "a1"}]}, "admission_id": "a1"}, {})
add("lost_admission", {"snapshot": {"pending": [{"admission_id": "a2"}]}, "admission_id": "a1"}, {})

# ── _oneshot_admission ──
add("oneshot_admission", {"receipt": {"admission_id": "a1"}}, {})
add("oneshot_admission", {"receipt": {"voice_stopped": True, "admission_id": "a1"}}, {})
add("oneshot_admission", {"receipt": {"voice_stopped": 1, "admission_id": "a1"}}, {})
add("oneshot_admission", {"receipt": None}, {})
add("oneshot_admission", {"receipt": {}}, {})
add("oneshot_admission", {"receipt": {"voice_stopped": False, "admission_id": "a2"}}, {})
add("oneshot_admission", {"receipt": "x"}, {})

# ── resume_footer ──
def footer_seams(profile, text, **over):
    s = {"import_t": None, "import_get_active_profile_name": None,
         "get_active_profile_name": profile, "t": text}
    s.update(over)
    return s


add("resume_footer", {"session_id": "s1"}, footer_seams("default", "Resume:"))
add("resume_footer", {"session_id": "s1"}, footer_seams("custom", "Resume:"))
add("resume_footer", {"session_id": "s2"}, footer_seams("bot1", "Resume:"))
add("resume_footer", {"session_id": "s1"}, footer_seams("default", rs("RuntimeError", "t")))
add("resume_footer", {"session_id": "s1"},
    footer_seams("default", "Resume:", get_active_profile_name=rs("ValueError", "profile")))
add("resume_footer", {"session_id": "s1"},
    {"import_t": None, "import_get_active_profile_name": None,
     "get_active_profile_name": "default"})  # fail-closed t
add("resume_footer", {"session_id": "s1"},
    footer_seams("default", "Resume:", import_t=rs("ImportError", "no i18n")))

# ── GatewayChatView._finite_block ──
add("finite_block", {"self": {"pending": [{"admission_id": "a1", "status": "unknown"}],
                              "completions": {}, "prompts": {}, "gaps": 0},
                     "admission": "a1"}, {})
add("finite_block", {"self": {"pending": [{"admission_id": "a1", "status": "queued"},
                                          {"admission_id": "a2", "status": "unknown"}],
                              "completions": {}, "prompts": {}, "gaps": 0},
                     "admission": "a1"}, {})
add("finite_block", {"self": {"pending": [{"admission_id": "a1", "status": "queued"}],
                              "completions": {}, "prompts": {}, "gaps": 0},
                     "admission": "a1"}, {})
add("finite_block", {"self": {"pending": [{"admission_id": "a1", "status": "running"}],
                              "completions": {}, "prompts": {"p1": {}}, "gaps": 0},
                     "admission": "a1"}, {})
add("finite_block", {"self": {"pending": [{"admission_id": "a1", "status": "running"}],
                              "completions": {}, "prompts": {"p1": {}}, "gaps": 1},
                     "admission": "a1"}, {})
add("finite_block", {"self": {"pending": [], "completions": {}, "prompts": {"p1": {}}, "gaps": 0},
                     "admission": "a1"}, {})
add("finite_block", {"self": {"pending": [], "completions": {}, "prompts": {"p1": {}}, "gaps": 1},
                     "admission": "a1"}, {})
add("finite_block", {"self": {"pending": [], "completions": {}, "prompts": {}, "gaps": 0},
                     "admission": "a1"}, {})
add("finite_block", {"self": {"pending": [{"admission_id": "a1"}],
                              "completions": {}, "prompts": {}, "gaps": 0},
                     "admission": "a1"}, {})  # KeyError status (matching row)
add("finite_block", {"self": {"pending": [{"admission_id": "a1", "status": "queued"},
                                          {"admission_id": "a2"}],
                              "completions": {}, "prompts": {}, "gaps": 0},
                     "admission": "a1"}, {})  # unknown_admissions KeyError status

# ── exit_single_query ──
add("exit_single_query", {"environ": {}, "code": 0}, {})
add("exit_single_query", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "code": 75},
    {"print": None})
add("exit_single_query", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "code": 1},
    {"print": None})
add("exit_single_query", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "code": 0},
    {"print": rs("RuntimeError", "print fail")})
add("exit_single_query", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "code": 0},
    {"print": rs("KeyboardInterrupt")})
add("exit_single_query", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "code": 0},
    {"print": rs("SystemExit", "9")})
add("exit_single_query", {"environ": {"HERMES_KANBAN_TASK": "t1"}, "code": 3}, {})  # fail-closed print
add("exit_single_query", {"environ": {"HERMES_KANBAN_TASK": ""}, "code": 2}, {})

# ── take_turn_report_path ──
add("take_turn_report_path", {"environ": {"HERMES_QUIET_TURN_REPORT_FILE": "/tmp/r.json"}}, {})
add("take_turn_report_path", {"environ": {}}, {})
add("take_turn_report_path", {"environ": {"HERMES_QUIET_TURN_REPORT_FILE": ""}}, {})
add("take_turn_report_path", {"environ": {"HERMES_QUIET_TURN_REPORT_FILE": None}}, {})
add("take_turn_report_path", {"environ": {"other": "x", "HERMES_QUIET_TURN_REPORT_FILE": "/r"}}, {})
add("take_turn_report_path", {"environ": {"HERMES_QUIET_TURN_REPORT_FILE": 0}}, {})

# ── _oneshot_exit_code ──
add("oneshot_exit_code", {"response": "x", "result": {"interrupted": True}}, {})
add("oneshot_exit_code", {"response": "x", "result": {"failed": True}}, {})
add("oneshot_exit_code", {"response": "x", "result": {"partial": True}}, {})
add("oneshot_exit_code", {"response": "x", "result": {"completed": False}}, {})
add("oneshot_exit_code", {"response": "hello", "result": {"completed": True}}, {})
add("oneshot_exit_code", {"response": "", "result": {"completed": True}}, {})
add("oneshot_exit_code", {"response": "   ", "result": {"completed": True}}, {})
add("oneshot_exit_code", {"response": None, "result": {"completed": True}}, {})
add("oneshot_exit_code", {"response": "x", "result": {}}, {})
add("oneshot_exit_code", {"response": "x", "result": None}, {})
add("oneshot_exit_code", {"response": "x", "result": {"completed": 0}}, {})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    thin = {k: v for k, v in counts.items() if v < 5}
    print("wrote %d cases (%d funcs) to %s" % (len(CASES), len(counts), OUT))
    if thin:
        print("WARNING <5 cases: %s" % thin)


if __name__ == "__main__":
    main()
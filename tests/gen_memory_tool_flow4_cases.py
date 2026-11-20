#!/usr/bin/env python3
"""Generate tests/fixtures/memory_tool_flow4.jsonl (memory_tool_flow4 oracle).

Each case is {"func": name, "in": {"args": {...}, "script": {...}}}.
The script maps seam name -> a single result or a per-call list (last repeats);
{"__raise__": Kind} models a raised Python exception. >=5 cases per function.
"""
import json
import os
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "memory_tool_flow4.jsonl")
CASES = []


def add(func, args=None, script=None):
    CASES.append({"func": func, "in": {"args": args or {}, "script": script or {}}})


# ---- batch_op_line (pure) ----
add("batch_op_line", {"op": {}})
add("batch_op_line", {"op": {"action": "remove", "old_text": "stale fact"}})
add("batch_op_line", {"op": {"action": "patch", "old_text": "old", "content": "new"}})
add("batch_op_line", {"op": {"action": "add", "content": "remember this"}})
add("batch_op_line", {"op": {"content": "new_text alias"}})
add("batch_op_line", {"op": {"action": "add", "new_text": "from alias", "old_text": "ignored"}})
add("batch_op_line", {})

# ---- destructive_ops (pure) ----
add("destructive_ops", {"payload": {"action": "add", "content": "x"}})
add("destructive_ops", {"payload": {"action": "remove", "old_text": "x"}})
add("destructive_ops", {"payload": {"action": "patch", "old_text": "x", "content": "y"}})
add("destructive_ops", {"payload": {"action": "batch", "operations": [
    {"action": "add", "content": "a"}, {"action": "remove", "old_text": "b"},
    {"action": "patch", "old_text": "c", "content": "d"}, {"action": "replace", "old_text": "e"}]}})
add("destructive_ops", {"payload": {"action": "batch", "operations": []}})
add("destructive_ops", {"payload": {}})
add("destructive_ops", {"payload": {"action": "batch", "operations": [None, {}, {"action": "add"}]}})

# ---- applied (pure) ----
add("applied", {"result": {"success": True, "target": "memory", "current_chars": 10}})
add("applied", {"result": {"success": False, "error": "full", "current_entries": ["a", "b"]}})
add("applied", {"result": {"success": True}})
add("applied", {"result": {"success": False}})
add("applied", {"result": {}})

# ---- invalid (pure formatter) ----
add("invalid", {"message": "Unknown action 'frobnicate'. Use: add, patch, remove"})
add("invalid", {"message": "Content is required for 'add' action."})
add("invalid", {"message": "quote \" and unicode ☃"})
add("invalid", {"message": ""})
add("invalid", {"message": "a" * 120})

# ---- validate_single_op ----
add("validate_single_op", {"action": "add", "target": "memory", "content": None})
add("validate_single_op", {"action": "add", "target": "user", "content": ""})
add("validate_single_op", {"action": "remove", "target": "memory", "old_text": None},
    {"store._entries_for": ["alpha", "beta"], "store._usage": "2/2200"})
add("validate_single_op", {"action": "patch", "target": "user", "old_text": ""},
    {"store._entries_for": [], "store._usage": "0/1375"})
add("validate_single_op", {"action": "patch", "target": "memory", "old_text": "x", "content": None})
add("validate_single_op", {"action": "add", "target": "memory", "content": "valid new entry"})
add("validate_single_op", {"action": "remove", "target": "memory", "old_text": "valid old"})
add("validate_single_op", {"action": "patch", "target": "memory", "old_text": "old", "content": "new"})
add("validate_single_op", {"action": "add", "target": "memory", "content": "x",
                           "script_unused": True})

# ---- pin_matched_entries ----
add("pin_matched_entries", {"payload": {"action": "batch", "target": "memory", "operations": [
    {"action": "remove", "old_text": "a"}, {"action": "patch", "old_text": "b", "content": "c"}]}},
    {"resolve_batch_entries": {"success": True, "matched_entries": ["full A", None]}})
add("pin_matched_entries", {"payload": {"action": "batch", "target": "user", "operations": [
    {"action": "remove", "old_text": "a"}]}},
    {"resolve_batch_entries": {"success": False, "error": "not found", "target": "user"}})
add("pin_matched_entries", {"payload": {"action": "remove", "target": "memory", "old_text": "a"}},
    {"resolve_entry": {"success": True, "matched_entry": "full entry A"}})
add("pin_matched_entries", {"payload": {"action": "patch", "target": "memory", "old_text": "b"}},
    {"resolve_entry": {"success": False, "error": "nomatch"}})
add("pin_matched_entries", {"payload": {"action": "add", "content": "x"}})
add("pin_matched_entries", {"payload": {"action": "batch", "operations": []}},
    {"resolve_batch_entries": {"success": True, "matched_entries": []}})
add("pin_matched_entries", {"payload": {"action": "remove", "old_text": "a"}},
    {"resolve_entry": {"__raise__": "RuntimeError", "msg": "boom"}})
add("pin_matched_entries", {"payload": {"action": "batch", "operations": [
    {"action": "remove"}]}},
    {"resolve_batch_entries": {"success": True, "matched_entries": [{"nested": 1}]}})

# ---- gate_or_stage ----
add("gate_or_stage", {"summary": "apply 1 op(s) to memory", "detail": "- remove: x",
                      "payload": {"action": "remove", "target": "memory", "old_text": "x"}},
    {"import write_approval": {"__raise__": "ImportError", "msg": "no wa"}, "evaluate_gate": {"allow": True}})
add("gate_or_stage", {"summary": "s", "detail": "d", "payload": {"action": "add"}},
    {"import write_approval": {"__raise__": "SystemExit", "msg": "bye"}})
add("gate_or_stage", {"summary": "s", "detail": "d", "payload": {"action": "add"}},
    {"import write_approval": None, "evaluate_gate": {"allow": True}})
add("gate_or_stage", {"summary": "s", "detail": "d", "payload": {"action": "add"}},
    {"evaluate_gate": {"blocked": True, "message": "denied by policy"}})
add("gate_or_stage", {"summary": "add to memory", "detail": "some detail",
                      "payload": {"action": "add", "target": "memory", "content": "x"}},
    {"evaluate_gate": {"allow": False, "message": "please approve"},
     "resolve_entry": {"success": True, "matched_entry": "E"},
     "stage_write": {"id": "pending-1"}, "current_origin": "cli"})
add("gate_or_stage", {"summary": "apply 2 op(s) to user profile", "detail": "- remove: a\n- patch: b",
                      "payload": {"action": "batch", "target": "user", "operations": [
                          {"action": "remove", "old_text": "a"}, {"action": "patch", "old_text": "b", "content": "c"}]}},
    {"evaluate_gate": {"message": "batch needs approval"},
     "resolve_batch_entries": {"success": True, "matched_entries": [None, None]},
     "stage_write": {"id": "pending-2"}, "current_origin": "gateway"})
add("gate_or_stage", {"summary": "s", "detail": "d",
                      "payload": {"action": "remove", "target": "memory", "old_text": "x"}},
    {"evaluate_gate": {"message": "approve"}, "resolve_entry": {"success": False, "error": "changed"}})
add("gate_or_stage", {"summary": "s", "detail": "d", "payload": {"action": "add"}},
    {"evaluate_gate": {"message": "approve"}, "stage_write": {"__raise__": "OSError", "msg": "disk"}})
add("gate_or_stage", {"summary": "s", "detail": "d", "payload": {"action": "add"}},
    {"evaluate_gate": {"__raise__": "ValueError", "msg": "bad gate"}})

# ---- background_delete_gate ----
add("background_delete_gate", {"action": "remove", "target": "memory", "old_text": "x"},
    {"import skill_provenance": None, "is_unattended_review": False})
add("background_delete_gate", {"action": "add", "content": "x"},
    {"is_unattended_review": True})
add("background_delete_gate", {"action": "remove", "target": "memory", "old_text": "x"},
    {"is_unattended_review": True, "resolve_entry": {"success": True, "matched_entry": "E"},
     "stage_write": {"id": "bg-1"}, "current_origin": "background"})
add("background_delete_gate", {"action": "patch", "target": "user", "old_text": "b", "content": "c"},
    {"is_unattended_review": True, "resolve_entry": {"success": False, "error": "changed"}})
add("background_delete_gate", {"action": "batch", "target": "memory", "operations": [
    {"action": "add", "content": "a"}, {"action": "remove", "old_text": "b"}]},
    {"is_unattended_review": True,
     "resolve_batch_entries": {"success": True, "matched_entries": [None, "full B"]},
     "stage_write": {"id": "bg-2"}, "current_origin": "background"})
add("background_delete_gate", {"action": "remove", "old_text": "x"},
    {"is_unattended_review": True, "resolve_entry": {"__raise__": "RuntimeError", "msg": "boom"}})
add("background_delete_gate", {"action": "remove", "old_text": "x"},
    {"is_unattended_review": True, "resolve_entry": {"success": True, "matched_entry": "E"},
     "import write_approval": {"__raise__": "RuntimeError", "msg": "no wa"}})
add("background_delete_gate", {"action": "remove", "old_text": "x"},
    {"is_unattended_review": True, "resolve_entry": {"success": True, "matched_entry": "E"},
     "stage_write": {"__raise__": "OSError", "msg": "disk"}})
add("background_delete_gate", {"action": "remove", "old_text": "x"},
    {"is_unattended_review": True, "resolve_entry": {"success": True, "matched_entry": "E"},
     "stage_write": {"__raise__": "SystemExit", "msg": "bye"}})
add("background_delete_gate", {"action": "remove", "old_text": "x"},
    {"import skill_provenance": {"__raise__": "SystemExit", "msg": "bye"}})

# ---- memory_tool ----
add("memory_tool", {"action": "add", "target": "memory", "content": "", "new_text": "aliased"},
    {"_memory_target_error": None, "is_unattended_review": False,
     "_apply_write_gate": None, "store.add": {"success": True, "target": "memory"}})
add("memory_tool", {"action": "add", "target": "memory", "content": "x"},
    {"_memory_target_error": {"success": False, "error": "disabled"}})
add("memory_tool", {"action": "add", "operations": "not-a-list", "target": "memory"},
    {"_memory_target_error": None})
add("memory_tool", {"action": "batch", "operations": [{"action": "replace", "old_text": "x"}],
                    "target": "memory"}, {"_memory_target_error": None})
add("memory_tool", {"action": "batch", "operations": [{"action": "remove", "old_text": "x"}],
                    "target": "memory"},
    {"_memory_target_error": None, "is_unattended_review": True,
     "resolve_batch_entries": {"success": True, "matched_entries": ["full entry X"]},
     "import write_approval": None,
     "stage_write": {"id": "bg-1"}, "current_origin": "background"})
add("memory_tool", {"action": "batch", "operations": [{"action": "add", "content": "x"}],
                    "target": "memory"},
    {"_memory_target_error": None, "is_unattended_review": False,
     "_apply_write_gate": {"success": False, "error": "approval required"},
     "apply_batch": {"success": True, "target": "memory"}})
add("memory_tool", {"action": "batch", "operations": [{"action": "add", "content": "x"}],
                    "target": "memory"},
    {"_memory_target_error": None, "is_unattended_review": False,
     "_apply_write_gate": None, "apply_batch": {"success": True, "count": 1}})
add("memory_tool", {"target": "memory"},
    {"_memory_target_error": None})
add("memory_tool", {"action": "replace", "target": "memory", "old_text": "x", "content": "y"},
    {"_memory_target_error": None})
add("memory_tool", {"action": "frobnicate", "target": "memory"},
    {"_memory_target_error": None})
add("memory_tool", {"action": "add", "target": "memory", "content": None},
    {"_memory_target_error": None, "failure_class.set": None})
add("memory_tool", {"action": "add", "target": "memory", "content": "x"},
    {"_memory_target_error": None, "is_unattended_review": False,
     "_apply_write_gate": {"success": False, "error": "staged"},
     "store.add": {"success": True}})
add("memory_tool", {"action": "remove", "target": "memory", "old_text": "x"},
    {"_memory_target_error": None, "is_unattended_review": False,
     "store._entries_for": ["a"], "store._usage": "1/2200",
     "_apply_write_gate": None, "store.remove": {"success": True, "removed": "x"}})
add("memory_tool", {"action": "add", "target": "memory", "content": "x"},
    {"_memory_target_error": None, "is_unattended_review": False,
     "_apply_write_gate": None, "store.add": {"__raise__": "RuntimeError", "msg": "disk"}})

# ---- except_dispatch (full builtin exception hierarchy) ----
ALL_NAMES = [
    "BaseException", "Exception", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
    "CancelledError", "BaseExceptionGroup", "ExceptionGroup", "TypeError", "ValueError",
    "AttributeError", "NameError", "LookupError", "ArithmeticError", "OSError",
    "EnvironmentError", "IOError", "WindowsError", "StopIteration", "StopAsyncIteration",
    "AssertionError", "SyntaxError", "SystemError", "ImportError", "Warning", "RuntimeError",
    "MemoryError", "BufferError", "EOFError", "ReferenceError", "MT4TestExc",
    "PythonFinalizationError", "NotImplementedError", "RecursionError", "UnboundLocalError",
    "UnicodeError", "UnicodeEncodeError", "IndexError", "KeyError", "ZeroDivisionError",
    "OverflowError", "FloatingPointError", "FileNotFoundError", "PermissionError",
    "ConnectionError", "BrokenPipeError", "ModuleNotFoundError", "IndentationError",
    "TabError", "UserWarning", "DeprecationWarning", "EncodingWarning", "__NotAnExc__",
]
add("except_dispatch", {"raised": "RuntimeError", "clauses": ALL_NAMES})
add("except_dispatch", {"raised": "ValueError", "clauses": ["Exception", "ValueError", "BaseException"]})
add("except_dispatch", {"raised": "FileNotFoundError", "clauses": ["OSError", "EnvironmentError", "IOError", "WindowsError"]})
add("except_dispatch", {"raised": "FileNotFoundError", "clauses": ["ValueError", "TypeError", "KeyError"]})
add("except_dispatch", {"raised": "KeyboardInterrupt", "clauses": ["Exception", "BaseException"]})
add("except_dispatch", {"raised": "SystemExit", "clauses": ["Exception", "ValueError", "BaseException"]})
add("except_dispatch", {"raised": "MT4TestExc", "clauses": ["RuntimeError", "Exception", "MT4TestExc"]})
add("except_dispatch", {"raised": "ImportError", "clauses": ["ModuleNotFoundError", "ImportError", "Exception"]})
add("except_dispatch", {"raised": "Exception", "clauses": ["BaseException", "Exception"]})
add("except_dispatch", {"raised": "OSError", "clauses": ["__NotAnExc__", "ValueError", "OSError"]})
add("except_dispatch", {"raised": "TypeError", "clauses": []})
add("except_dispatch", {"raised": "ZeroDivisionError", "clauses": ["ArithmeticError", "ZeroDivisionError", "Exception"]})

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

counts = Counter(c["func"] for c in CASES)
print("wrote %s: %d cases (%s)" % (OUT, len(CASES), dict(counts)))

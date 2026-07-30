#!/usr/bin/env python3
"""Oracle: tools/file_operations.py search-diagnostics helpers vs LIVE Python.
Newline-delimited JSON; compares C output to live Python for identical inputs."""
import json, sys, os, types
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import importlib.util
spec = importlib.util.spec_from_file_location("fo_mod", "/home/wubu/hermes-agent-dev/tools/file_operations.py")
mod = importlib.util.module_from_spec(spec)
for name in ["agent.tool_result_classification", "tools.terminal", "tools.patch",
             "tools.file_lint", "tools.memory", "agent.skills", "utils", "hermes_constants"]:
    m = types.ModuleType(name)
    if name == "agent.tool_result_classification":
        m.FILE_MUTATING_TOOL_NAMES = {"write_file", "patch"}
    sys.modules[name] = m
try:
    spec.loader.exec_module(mod)
except Exception as e:
    print("IMPORT_FAIL", repr(e)); sys.exit(2)

class R:
    def __init__(self, code, so): self.exit_code = code; self.stdout = so; self.stderr = ""

OUT = ("rg: /foo: Permission denied\n"
       "src/main.c:10:int main()\ngrep: Invalid regular expression\n"
       "src/util.c:5:void x()\n--\nsomepath/file-12-name.py-8-context")

mm = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    b = json.loads(line)
    if b["t"] == "search_limit":
        exp = mod._search_stdout_and_limit(R(124, "line1\n[Command timed out after 30s]\nline2"))
        got = (b["stdout"], b["reason"])
        if (exp[0], exp[1]) != got: mm += 1; print(f"MISMATCH search_limit: exp {exp} got {got}")
    elif b["t"] == "search_limit2":
        exp = mod._search_stdout_and_limit(R(0, "normal output"))
        got = (b["stdout"], b["reason"])
        if (exp[0], exp[1]) != got: mm += 1; print(f"MISMATCH search_limit2: exp {exp} got {got}")
    elif b["t"] == "split_diag":
        exp = mod._split_tool_diagnostics(OUT)
        got = (b["diag"], b["pay"])
        if exp != got: mm += 1; print(f"MISMATCH split_diag:\nEXP:{exp}\nGOT:{got}")
if mm:
    print(f"oracle: {mm} mismatch(es)"); sys.exit(1)
print("oracle: 0 mismatches")

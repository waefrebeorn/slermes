"""twin: system_prompt_parts prefix block from a real RuntimeMode, workspace pinned to "".

workspace_block="" means no git probe runs, so the prefix is the only thing under test.
"""
from pathlib import Path
import agent.coding_context as _cc
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "system_prompt_parts"
    C_FN = "coding_context_build_brief"
    def call(self, inp, inputs, idx=0):
        model, _, todo_s = inp.partition("|")
        has_todo = int(todo_s)
        valid = None if has_todo == -1 else (["todo_list", "read_file"] if has_todo == 1 else ["read_file"])
        m = _cc.RuntimeMode(profile=_cc.CODING_PROFILE, surface="cli", cwd=Path("."),
                            config_mode="focus", model=(None if model == "None" else model),
                            instructions="")
        prefix, _ws, _tr = m.system_prompt_parts(valid_tool_names=valid, workspace_block="")
        return prefix[0] if prefix else ""
    def norm(self, c, result):
        c.valstr(result.encode("utf-8").hex())
_t=_Twin(); trace=_t.trace; run=_t.run

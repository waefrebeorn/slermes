"""twin for agent/system_prompt.py:build_system_prompt_parts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/system_prompt.py"
    PY_FN = "build_system_prompt_parts"
    C_FN = "build_system_prompt_parts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

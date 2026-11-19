"""twin for agent/memory_provider.py:system_prompt_block (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/memory_provider.py"
    PY_FN = "system_prompt_block"
    C_FN = "system_prompt_block"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

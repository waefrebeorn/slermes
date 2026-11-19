"""twin for agent/shell_hooks.py:_prompt_and_record (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/shell_hooks.py"
    PY_FN = "_prompt_and_record"
    C_FN = "shk_prompt_and_record"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

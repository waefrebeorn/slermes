"""twin for agent/prompt_builder.py:_read_text_with_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_builder.py"
    PY_FN = "_read_text_with_timeout"
    C_FN = "_read_text_with_timeout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

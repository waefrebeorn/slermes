"""twin for tools/transcription_tools.py:_enforce_prompt_length_limit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_enforce_prompt_length_limit"
    C_FN = "_enforce_prompt_length_limit"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for tools/wake_word.py:enrolled_profile_phrases (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/wake_word.py"
    PY_FN = "enrolled_profile_phrases"
    C_FN = "enrolled_profile_phrases"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

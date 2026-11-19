"""twin for agent/side_question.py:_answer_via_oneshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/side_question.py"
    PY_FN = "_answer_via_oneshot"
    C_FN = "_answer_via_oneshot"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

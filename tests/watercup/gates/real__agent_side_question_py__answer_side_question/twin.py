"""twin for agent/side_question.py:answer_side_question (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/side_question.py"
    PY_FN = "answer_side_question"
    C_FN = "answer_side_question"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

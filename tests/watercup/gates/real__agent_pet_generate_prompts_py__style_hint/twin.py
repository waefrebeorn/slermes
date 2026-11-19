"""twin for agent/pet/generate/prompts.py:style_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/prompts.py"
    PY_FN = "style_hint"
    C_FN = "style_hint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

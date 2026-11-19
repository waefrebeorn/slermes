"""twin for agent/image_gen_provider.py:generate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/image_gen_provider.py"
    PY_FN = "generate"
    C_FN = "generate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

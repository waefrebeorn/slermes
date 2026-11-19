"""twin for agent/image_gen_provider.py:resolve_aspect_ratio (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/image_gen_provider.py"
    PY_FN = "resolve_aspect_ratio"
    C_FN = "resolve_aspect_ratio"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

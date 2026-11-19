"""twin for agent/auxiliary_client.py:_resolve_strict_vision_backend (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_resolve_strict_vision_backend"
    C_FN = "_resolve_strict_vision_backend"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

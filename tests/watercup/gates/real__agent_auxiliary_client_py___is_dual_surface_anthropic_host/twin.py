"""twin for agent/auxiliary_client.py:_is_dual_surface_anthropic_host (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_is_dual_surface_anthropic_host"
    C_FN = "_is_dual_surface_anthropic_host"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

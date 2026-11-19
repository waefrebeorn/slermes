"""twin for agent/verify_hooks.py:_agent_cfg (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify_hooks.py"
    PY_FN = "_agent_cfg"
    C_FN = "_agent_cfg"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

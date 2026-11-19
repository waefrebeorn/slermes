"""twin for agent/copilot_acp_client.py:_resolve_args (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/copilot_acp_client.py"
    PY_FN = "_resolve_args"
    C_FN = "_resolve_args"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

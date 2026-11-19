"""twin for agent/copilot_acp_client.py:_build_subprocess_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/copilot_acp_client.py"
    PY_FN = "_build_subprocess_env"
    C_FN = "_build_subprocess_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

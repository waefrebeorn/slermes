"""twin for agent/auxiliary_client.py:_try_azure_foundry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_try_azure_foundry"
    C_FN = "_try_azure_foundry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

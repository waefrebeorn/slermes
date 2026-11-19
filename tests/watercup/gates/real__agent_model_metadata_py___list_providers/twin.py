"""twin for agent/model_metadata.py:_list_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "_list_providers"
    C_FN = "_list_providers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

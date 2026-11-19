"""twin for agent/models_dev.py:_fetch_models_dev_from_network (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_fetch_models_dev_from_network"
    C_FN = "_fetch_models_dev_from_network"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

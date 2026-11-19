"""twin for agent/models_dev.py:_models_dev_to_hermes_ids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_models_dev_to_hermes_ids"
    C_FN = "_models_dev_to_hermes_ids"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

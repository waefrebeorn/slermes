"""twin for agent/model_metadata.py:fetch_endpoint_model_metadata (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"
    PY_FN = "fetch_endpoint_model_metadata"
    C_FN = "fetch_endpoint_model_metadata"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

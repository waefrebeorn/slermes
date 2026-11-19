"""twin for agent/auxiliary_client.py:_nous_min_key_ttl_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_nous_min_key_ttl_seconds"
    C_FN = "auxc_nous_min_key_ttl_seconds"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

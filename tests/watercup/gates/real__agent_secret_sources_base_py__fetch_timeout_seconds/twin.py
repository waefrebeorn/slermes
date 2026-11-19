"""twin for agent/secret_sources/base.py:fetch_timeout_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/base.py"
    PY_FN = "fetch_timeout_seconds"
    C_FN = "ssb_fetch_timeout_seconds"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

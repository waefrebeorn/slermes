"""twin for agent/credential_pool.py:_warn_env_ingestion_once (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "_warn_env_ingestion_once"
    C_FN = "_warn_env_ingestion_once"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

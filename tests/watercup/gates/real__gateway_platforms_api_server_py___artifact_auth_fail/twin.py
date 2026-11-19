"""twin for gateway/platforms/api_server.py:_artifact_auth_fail (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_artifact_auth_fail"
    C_FN = "_artifact_auth_fail"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for gateway/platforms/api_server.py:_release_run_owner_if_forgotten (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_release_run_owner_if_forgotten"
    C_FN = "_release_run_owner_if_forgotten"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

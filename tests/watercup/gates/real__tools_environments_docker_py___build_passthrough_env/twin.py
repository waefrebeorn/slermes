"""twin for tools/environments/docker.py:_build_passthrough_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/docker.py"
    PY_FN = "_build_passthrough_env"
    C_FN = "_build_passthrough_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

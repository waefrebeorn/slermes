"""twin for tools/environments/docker.py:_build_runtime_env_args (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/docker.py"
    PY_FN = "_build_runtime_env_args"
    C_FN = "_build_runtime_env_args"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

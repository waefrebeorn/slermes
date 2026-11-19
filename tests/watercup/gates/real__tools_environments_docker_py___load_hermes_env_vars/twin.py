"""twin for tools/environments/docker.py:_load_hermes_env_vars (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/docker.py"
    PY_FN = "_load_hermes_env_vars"
    C_FN = "envd_u_load_hermes_env_vars"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

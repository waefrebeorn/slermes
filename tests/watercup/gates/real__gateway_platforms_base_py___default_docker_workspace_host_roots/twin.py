"""twin for gateway/platforms/base.py:_default_docker_workspace_host_roots (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_default_docker_workspace_host_roots"
    C_FN = "_default_docker_workspace_host_roots"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

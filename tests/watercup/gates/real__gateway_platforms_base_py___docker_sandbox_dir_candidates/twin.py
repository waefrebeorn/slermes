"""twin for gateway/platforms/base.py:_docker_sandbox_dir_candidates (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_docker_sandbox_dir_candidates"
    C_FN = "_docker_sandbox_dir_candidates"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

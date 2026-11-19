"""twin for gateway/platforms/base.py:_warn_unresolved_docker_media (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "_warn_unresolved_docker_media"
    C_FN = "_warn_unresolved_docker_media"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

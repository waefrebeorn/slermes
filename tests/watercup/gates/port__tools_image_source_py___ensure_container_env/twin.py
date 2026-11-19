"""twin for tools/image_source.py:_ensure_container_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/image_source.py"
    PY_FN = "_ensure_container_env"
    C_FN = "_ensure_container_env"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

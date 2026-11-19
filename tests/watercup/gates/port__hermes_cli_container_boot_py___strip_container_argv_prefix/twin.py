"""twin for hermes_cli/container_boot.py:_strip_container_argv_prefix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/container_boot.py"
    PY_FN = "_strip_container_argv_prefix"
    C_FN = "_strip_container_argv_prefix"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

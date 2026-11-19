"""twin for hermes_cli/config.py:_ensure_default_soul_md (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "_ensure_default_soul_md"
    C_FN = "config_py_ensure_default_soul_md"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

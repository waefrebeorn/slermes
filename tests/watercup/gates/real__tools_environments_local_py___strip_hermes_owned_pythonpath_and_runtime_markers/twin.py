"""twin for tools/environments/local.py:_strip_hermes_owned_pythonpath_and_runtime_markers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/local.py"
    PY_FN = "_strip_hermes_owned_pythonpath_and_runtime_markers"
    C_FN = "_strip_hermes_owned_pythonpath_and_runtime_markers"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

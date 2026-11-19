"""twin for tools/environments/local.py:_managed_runtime_path_entries (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/local.py"
    PY_FN = "_managed_runtime_path_entries"
    C_FN = "_managed_runtime_path_entries"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for tools/computer_use/doctor.py:_normalize_version_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/doctor.py"
    PY_FN = "_normalize_version_token"
    C_FN = "_normalize_version_token"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

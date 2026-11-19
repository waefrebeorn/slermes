"""twin for gateway/media_repair.py:_computer_use_capture_basename (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/media_repair.py"
    PY_FN = "_computer_use_capture_basename"
    C_FN = "_computer_use_capture_basename"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

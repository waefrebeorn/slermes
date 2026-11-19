"""twin for gateway/media_repair.py:repair_explicit_computer_use_media_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/media_repair.py"
    PY_FN = "repair_explicit_computer_use_media_paths"
    C_FN = "repair_explicit_computer_use_media_paths"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

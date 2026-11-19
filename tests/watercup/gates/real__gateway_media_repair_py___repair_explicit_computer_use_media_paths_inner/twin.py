"""twin for gateway/media_repair.py:_repair_explicit_computer_use_media_paths_inner (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/media_repair.py"
    PY_FN = "_repair_explicit_computer_use_media_paths_inner"
    C_FN = "_repair_explicit_computer_use_media_paths_inner"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for gateway/channel_directory.py:_directory_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/channel_directory.py"
    PY_FN = "_directory_path"
    C_FN = "_directory_path"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

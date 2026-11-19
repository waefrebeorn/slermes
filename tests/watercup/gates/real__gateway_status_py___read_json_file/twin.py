"""twin for gateway/status.py:_read_json_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "_read_json_file"
    C_FN = "_read_json_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

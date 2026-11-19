"""twin for gateway/platforms/signal.py:_parse_comma_list (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal.py"
    PY_FN = "_parse_comma_list"
    C_FN = "_parse_comma_list"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

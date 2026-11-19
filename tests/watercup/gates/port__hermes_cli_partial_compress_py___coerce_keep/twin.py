"""twin for hermes_cli/partial_compress.py:_coerce_keep (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/partial_compress.py"
    PY_FN = "_coerce_keep"
    C_FN = "cmd_compress_coerce_keep_value"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

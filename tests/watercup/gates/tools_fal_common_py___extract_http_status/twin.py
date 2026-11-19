"""twin for tools/fal_common.py:_extract_http_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/fal_common.py"
    PY_FN = "_extract_http_status"
    C_FN = "cli_tools_fal_common__extract_http_status"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

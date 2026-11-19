"""twin for tools/yuanbao_tools.py:_check_yuanbao (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/yuanbao_tools.py"
    PY_FN = "_check_yuanbao"
    C_FN = "cli_tools_yuanbao_tools__check_yuanbao"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

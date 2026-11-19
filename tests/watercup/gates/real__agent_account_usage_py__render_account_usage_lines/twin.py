"""twin for agent/account_usage.py:render_account_usage_lines (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/account_usage.py"
    PY_FN = "render_account_usage_lines"
    C_FN = "render_account_usage_lines"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

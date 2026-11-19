"""twin for hermes_cli/fallback_cmd.py:cmd_fallback_add (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/fallback_cmd.py"
    PY_FN = "cmd_fallback_add"
    C_FN = "cmd_fallback_add"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

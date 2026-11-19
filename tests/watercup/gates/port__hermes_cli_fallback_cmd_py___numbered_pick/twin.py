"""twin for hermes_cli/fallback_cmd.py:_numbered_pick (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/fallback_cmd.py"
    PY_FN = "_numbered_pick"
    C_FN = "hermes_cli_fallback_cmd_u_numbered_pick"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

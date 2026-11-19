"""twin for hermes_cli/portal_cli.py:_cmd_open (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/portal_cli.py"
    PY_FN = "_cmd_open"
    C_FN = "hermes_cli_portal_cli_u_cmd_open"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

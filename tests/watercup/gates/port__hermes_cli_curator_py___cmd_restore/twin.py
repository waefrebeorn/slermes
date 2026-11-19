"""twin for hermes_cli/curator.py:_cmd_restore (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/curator.py"
    PY_FN = "_cmd_restore"
    C_FN = "hermes_cli_curator_u_cmd_restore"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

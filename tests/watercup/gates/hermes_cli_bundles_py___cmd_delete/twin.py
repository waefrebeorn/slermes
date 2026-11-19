"""twin for hermes_cli/bundles.py:_cmd_delete (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/bundles.py"
    PY_FN = "_cmd_delete"
    C_FN = "hermes_cli_bundles_u_cmd_delete"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

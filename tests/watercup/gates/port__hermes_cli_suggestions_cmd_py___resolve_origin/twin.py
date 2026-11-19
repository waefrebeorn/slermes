"""twin for hermes_cli/suggestions_cmd.py:_resolve_origin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/suggestions_cmd.py"
    PY_FN = "_resolve_origin"
    C_FN = "hermes_cli_suggestions_cmd_u_resolve_origin"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

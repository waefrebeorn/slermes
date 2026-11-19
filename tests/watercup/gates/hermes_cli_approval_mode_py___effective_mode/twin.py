"""twin for hermes_cli/approval_mode.py:_effective_mode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/approval_mode.py"
    PY_FN = "_effective_mode"
    C_FN = "hermes_cli_approval_mode_u_effective_mode"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

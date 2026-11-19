"""twin for hermes_cli/relaunch.py:_extract_inherited_flags (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/relaunch.py"
    PY_FN = "_extract_inherited_flags"
    C_FN = "hermes_cli_relaunch_u_extract_inherited_flags"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

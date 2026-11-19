"""twin for hermes_cli/relaunch.py:_build_inherited_flag_table (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/relaunch.py"
    PY_FN = "_build_inherited_flag_table"
    C_FN = "hermes_cli_relaunch_u_build_inherited_flag_table"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

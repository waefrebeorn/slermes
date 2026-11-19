"""twin for hermes_cli/moa_cmd.py:_pick_slot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/moa_cmd.py"
    PY_FN = "_pick_slot"
    C_FN = "hermes_cli_moa_cmd_u_pick_slot"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

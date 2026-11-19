"""twin for hermes_cli/fallback_cmd.py:_restore_model_cfg (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/fallback_cmd.py"
    PY_FN = "_restore_model_cfg"
    C_FN = "hermes_cli_fallback_cmd_u_restore_model_cfg"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

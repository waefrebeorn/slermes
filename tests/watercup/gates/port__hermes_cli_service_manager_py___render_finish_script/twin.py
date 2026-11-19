"""twin for hermes_cli/service_manager.py:_render_finish_script (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/service_manager.py"
    PY_FN = "_render_finish_script"
    C_FN = "hermes_cli_service_manager_u_render_finish_script"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

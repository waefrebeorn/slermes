"""twin for tools/computer_use/permissions.py:_resolve_driver_cmd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/permissions.py"
    PY_FN = "_resolve_driver_cmd"
    C_FN = "tools_computer_use_permissions_u_resolve_driver_cmd"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

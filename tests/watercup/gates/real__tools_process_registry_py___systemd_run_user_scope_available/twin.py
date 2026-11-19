"""twin for tools/process_registry.py:_systemd_run_user_scope_available (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/process_registry.py"
    PY_FN = "_systemd_run_user_scope_available"
    C_FN = "process_registry_systemd_run_user_scope_available"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

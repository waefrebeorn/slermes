"""twin for hermes_cli/container_boot.py:_write_reconcile_log (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/container_boot.py"
    PY_FN = "_write_reconcile_log"
    C_FN = "hermes_cli_container_boot_u_write_reconcile_log"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

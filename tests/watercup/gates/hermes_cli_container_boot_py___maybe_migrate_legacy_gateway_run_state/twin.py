"""twin for hermes_cli/container_boot.py:_maybe_migrate_legacy_gateway_run_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/container_boot.py"
    PY_FN = "_maybe_migrate_legacy_gateway_run_state"
    C_FN = "hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

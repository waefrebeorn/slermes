"""twin for hermes_cli/main.py:_sweep_stale_bytecode_if_checkout_changed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_sweep_stale_bytecode_if_checkout_changed"
    C_FN = "main_g_sweep_stale_bytecode_if_checkout_changed"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

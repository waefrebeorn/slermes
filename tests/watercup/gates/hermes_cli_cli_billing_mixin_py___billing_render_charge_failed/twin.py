"""twin for hermes_cli/cli_billing_mixin.py:_billing_render_charge_failed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_billing_mixin.py"
    PY_FN = "_billing_render_charge_failed"
    C_FN = "cbm_billing_render_charge_failed"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

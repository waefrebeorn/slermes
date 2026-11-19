"""twin for hermes_cli/gateway_windows.py:_print_start_attestation_warning (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway_windows.py"
    PY_FN = "_print_start_attestation_warning"
    C_FN = "_print_start_attestation_warning"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

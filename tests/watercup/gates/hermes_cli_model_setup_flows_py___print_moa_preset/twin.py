"""twin for hermes_cli/model_setup_flows.py:_print_moa_preset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_setup_flows.py"
    PY_FN = "_print_moa_preset"
    C_FN = "msf_u_print_moa_preset"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

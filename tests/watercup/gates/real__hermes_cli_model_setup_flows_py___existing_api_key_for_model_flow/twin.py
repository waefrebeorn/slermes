"""twin for hermes_cli/model_setup_flows.py:_existing_api_key_for_model_flow (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_setup_flows.py"
    PY_FN = "_existing_api_key_for_model_flow"
    C_FN = "msf_u_existing_api_key_for_model_flow"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

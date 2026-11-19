"""twin for hermes_cli/model_setup_flows.py:bedrock_model_routable_from_region (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_setup_flows.py"
    PY_FN = "bedrock_model_routable_from_region"
    C_FN = "msf_bedrock_model_routable_from_region"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

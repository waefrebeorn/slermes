"""twin for gateway/response_filters.py:is_intentional_silence_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/response_filters.py"
    PY_FN = "is_intentional_silence_response"
    C_FN = "cli_gateway_response_filters_is_intentional_silence_response"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

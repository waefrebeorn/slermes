"""twin for gateway/run.py:_looks_like_gateway_provider_error/_gateway_provider_error_reply/_strip_auto_continue_noise (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_looks_like_gateway_provider_error/_gateway_provider_error_reply/_strip_auto_continue_noise"
    C_FN = "_looks_like_gateway_provider_error/_gateway_provider_error_reply/_strip_auto_continue_noise"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

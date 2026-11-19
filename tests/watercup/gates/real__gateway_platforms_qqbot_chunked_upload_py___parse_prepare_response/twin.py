"""twin for gateway/platforms/qqbot/chunked_upload.py:_parse_prepare_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/chunked_upload.py"
    PY_FN = "_parse_prepare_response"
    C_FN = "gateway_platforms_qqbot_chunke_u_parse_prepare_response"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

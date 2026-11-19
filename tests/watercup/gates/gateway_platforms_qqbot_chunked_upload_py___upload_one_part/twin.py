"""twin for gateway/platforms/qqbot/chunked_upload.py:_upload_one_part (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/chunked_upload.py"
    PY_FN = "_upload_one_part"
    C_FN = "gateway_platforms_qqbot_chunke_u_upload_one_part"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

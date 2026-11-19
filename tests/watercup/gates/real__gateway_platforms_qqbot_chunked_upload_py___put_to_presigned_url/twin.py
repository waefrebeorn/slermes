"""twin for gateway/platforms/qqbot/chunked_upload.py:_put_to_presigned_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/chunked_upload.py"
    PY_FN = "_put_to_presigned_url"
    C_FN = "gateway_platforms_qqbot_chunke_u_put_to_presigned_url"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

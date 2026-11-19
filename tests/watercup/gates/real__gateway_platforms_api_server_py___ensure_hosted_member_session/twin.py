"""twin for gateway/platforms/api_server.py:_ensure_hosted_member_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_ensure_hosted_member_session"
    C_FN = "_ensure_hosted_member_session"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

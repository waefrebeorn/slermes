"""twin for gateway/platforms/yuanbao_proto.py:encode_auth_bind (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_proto.py"
    PY_FN = "encode_auth_bind"
    C_FN = "encode_auth_bind"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for gateway/platforms/yuanbao_proto.py:_encode_send_c2c_req (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_proto.py"
    PY_FN = "_encode_send_c2c_req"
    C_FN = "_encode_send_c2c_req"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

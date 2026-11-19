"""twin for gateway/platforms/yuanbao_proto.py:_encode_msg_body_element (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_proto.py"
    PY_FN = "_encode_msg_body_element"
    C_FN = "yb_encode_msg_body_element"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

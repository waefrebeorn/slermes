"""twin for gateway/platforms/yuanbao_proto.py:encode_conn_msg (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_proto.py"
    PY_FN = "encode_conn_msg"
    C_FN = "encode_conn_msg"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

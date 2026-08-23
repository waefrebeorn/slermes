"""twin for yuanbao_proto_decode_msg_content.

Traces the LIVE gateway/platforms/yuanbao_proto.py::_decode_msg_content with
the SAME rung ladder as the C harness (which drives the REAL C decoder), and
hashes the decoded content in the same canonical sorted-key form.
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_yuanbao_proto",
    os.path.join(DEV, "gateway", "platforms", "yuanbao_proto.py"))
_yp = ilu.module_from_spec(_spec)
sys.modules["wc_live_yuanbao_proto"] = _yp
if _spec.loader is not None:
    _spec.loader.exec_module(_yp)


def canon(v):
    if isinstance(v, dict):
        return ";".join("%s=%s" % (k, canon(val)) for k, val in sorted(v.items()))
    if isinstance(v, list):
        return "|".join(canon(x) for x in v)
    if isinstance(v, bool):
        return "True" if v else "False"
    return str(v)


def trace(data):
    c = Cup("decode_msg_content")
    c.branch(len(data) > 0)                                        # B1
    content = _yp._decode_msg_content(data) if data else {}
    c.valstr(canon(content))
    return c.finalize()


def run(inputs):
    return [trace(bytes.fromhex(h)) for h in inputs]

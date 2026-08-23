"""twin for yuanbao_proto_encode_head.

Imports LIVE gateway/platforms/yuanbao_proto.py::_encode_head and traces the
SAME per-field branch ladder as the C harness; V carries the hex of the LIVE
bytes so any wire-format divergence fails the cup.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
DEV = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE)))))
sys.path.insert(0, DEV)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_yb_proto", os.path.join(DEV, "gateway", "platforms", "yuanbao_proto.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_yb_proto"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for inp in inputs:
        parts = inp.split("\t")
        while len(parts) < 7:
            parts.append("")
        f = [p.strip() for p in parts[:7]]
        cmd_type = int(f[0] or 0)
        cmd = f[1]
        seq_no = int(f[2] or 0)
        msg_id = f[3]
        module = f[4]
        need_ack = bool(int(f[5] or 0))
        status = int(f[6] or 0)

        c = Cup("_encode_head")
        c.branch(cmd_type != 0)
        c.branch(bool(cmd))
        c.branch(seq_no != 0)
        c.branch(bool(msg_id))
        c.branch(bool(module))
        c.branch(bool(need_ack))
        c.branch(status != 0)
        buf = mod._encode_head(cmd_type, cmd, seq_no, msg_id, module, need_ack=need_ack,
                               status=status & 0xFFFFFFFF)
        c.valstr(buf.hex())
        cups.append(c.finalize())
    return cups

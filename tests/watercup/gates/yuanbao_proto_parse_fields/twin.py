"""twin for yuanbao_proto_parse_fields.

Traces the LIVE gateway/platforms/yuanbao_proto.py::_parse_fields with the
same rung order as the C harness (input: hex-encoded protobuf bytes):
  per-record LOOP / B1 tag varint / B2 VARINT / B3 LEN / B4 64BIT /
  B5 32BIT / B6 unknown wire type -> ValueError stop.
Field values rendered as "<fn>:<wt>:<hex-or-int>" exactly like the C side.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_yb_proto", os.path.join(DEV, "gateway", "platforms", "yuanbao_proto.py"))
_yp = ilu.module_from_spec(_spec)
sys.modules["wc_live_yb_proto"] = _yp
try:
    _spec.loader.exec_module(_yp)
    live_parse = _yp._parse_fields
except Exception:
    live_parse = None

WT_VARINT, WT_64BIT, WT_LEN, WT_32BIT = 0, 1, 2, 5


def trace(data):
    c = Cup("parse_fields")
    fields = []
    pos = 0
    n = len(data)
    err = None
    while pos < n:
        c.loop()                                     # LOOP
        try:
            if live_parse is not None:
                # LIVE semantics for one record via the module's decoder
                tag, pos2 = _yp._decode_varint(data, pos)
            else:
                raise RuntimeError("no live module")
        except Exception as e:
            c.branch(False)                          # B1 fail
            break
        c.branch(True)                               # B1 ok
        pos = pos2
        fn = tag >> 3
        wt = tag & 0x07
        if wt == WT_VARINT:                          # B2
            c.branch(True)
            val, pos = _yp._decode_varint(data, pos)
            vbuf = "%d:0:%d" % (fn, val)
            fields.append((fn, wt, str(val)))
        elif wt == WT_LEN:                           # B3
            c.branch(False); c.branch(True)
            length, pos = _yp._decode_varint(data, pos)
            raw = data[pos:pos + length]
            pos += length
            vbuf = "%d:2:%s" % (fn, raw.hex())
            fields.append((fn, wt, raw.hex()))
        elif wt == WT_64BIT:                         # B4
            c.branch(False); c.branch(False); c.branch(True)
            raw = data[pos:pos + 8]
            if len(raw) < 8:
                break
            pos += 8
            vbuf = "%d:1:%s" % (fn, raw.hex())
            fields.append((fn, wt, raw.hex()))
        elif wt == WT_32BIT:                         # B5
            c.branch(False); c.branch(False); c.branch(False); c.branch(True)
            raw = data[pos:pos + 4]
            if len(raw) < 4:
                break
            pos += 4
            vbuf = "%d:5:%s" % (fn, raw.hex())
            fields.append((fn, wt, raw.hex()))
        else:                                        # B6 unknown
            c.branch(False); c.branch(False); c.branch(False); c.branch(False)
            try:
                live_parse(bytes(data))
            except Exception:
                pass
            c.valstr("ERR:%d" % wt)
            return err, c.finalize()
        c.valstr(vbuf)                               # V per field
    return err, c.finalize()


def run(inputs):
    cups = []
    for h in inputs:
        data = bytes.fromhex(h) if h else b""
        _, cup = trace(data)
        cups.append(cup)
    return cups

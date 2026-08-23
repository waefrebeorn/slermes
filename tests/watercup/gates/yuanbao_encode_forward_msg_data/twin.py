"""twin for yuanbao_encode_forward_msg_data — LIVE
gateway/platforms/yuanbao_proto.py::encode_forward_msg_data."""
import os, sys, json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_yb", os.path.join(DEV, "gateway", "platforms", "yuanbao_proto.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_yb"] = mod
        spec.loader.exec_module(mod)
        return mod.encode_forward_msg_data
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(data):
    WT_VARINT, WT_LEN = 0, 2

    def varint(value):
        if value < 0:
            value &= 0xFFFFFFFFFFFFFFFF
        out = bytearray()
        while True:
            bits = value & 0x7F
            value >>= 7
            if value:
                out.append(bits | 0x80)
            else:
                out.append(bits)
                return bytes(out)

    def field(fn, wt, val):
        return varint((fn << 3) | wt) + val

    def string(s):
        e = s.encode("utf-8")
        return varint(len(e)) + e

    def message(b):
        return varint(len(b)) + b

    def multimedia(media):
        buf = b""
        for fn, key in [(1, "type"), (2, "url"), (4, "file_name"), (15, "media_id")]:
            v = media.get(key, "")
            if v:
                buf += field(fn, WT_LEN, string(str(v)))
        for fn, key in [(5, "file_size"), (6, "width"), (7, "height")]:
            v = media.get(key, 0)
            if v:
                buf += field(fn, WT_VARINT, varint(int(v)))
        return buf

    def content(ct):
        buf = field(1, WT_VARINT, varint(int(ct.get("type", 0))))
        text = ct.get("text", "")
        if text:
            buf += field(2, WT_LEN, string(str(text)))
        for m in ct.get("multimedia") or []:
            buf += field(3, WT_LEN, message(multimedia(m)))
        return buf

    def msg(msg_d):
        buf = b""
        sender = msg_d.get("sender", "")
        if sender:
            buf += field(1, WT_LEN, string(str(sender)))
        tv = msg_d.get("time", 0)
        if tv:
            buf += field(2, WT_VARINT, varint(int(tv)))
        plain = msg_d.get("plainText", "")
        if plain:
            buf += field(3, WT_LEN, string(str(plain)))
        for mc in msg_d.get("msgContent") or []:
            buf += field(4, WT_LEN, message(content(mc)))
        return buf

    buf = field(1, WT_VARINT, varint(int(data.get("sub_type", 0))))
    for fn, key in [(2, "begin_time"), (3, "end_time")]:
        v = data.get(key, 0)
        if v:
            buf += field(fn, WT_VARINT, varint(int(v)))
    nick = data.get("nick_name", "")
    if nick:
        buf += field(4, WT_LEN, string(str(nick)))
    for m in data.get("msg") or []:
        buf += field(5, WT_LEN, message(msg(m)))
    return buf


def trace(raw):
    try:
        data = json.loads(raw)
    except Exception:
        data = None

    c = Cup("encode_forward_msg_data")
    c.branch(isinstance(data, dict))                            # B1
    if isinstance(data, dict):
        c.branch(bool(data.get("begin_time", 0)))               # B2
        c.branch(bool(data.get("end_time", 0)))                 # B3
        c.branch(bool(data.get("nick_name", "")))               # B4
        c.branch(bool(data.get("msg")))                         # B5

    result = (_live_fn(data) if (_live_fn and isinstance(data, dict))
              else (_fallback(data) if isinstance(data, dict) else b""))
    c.valstr(result.hex())
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]

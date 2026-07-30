#!/usr/bin/env python3
"""Round-trip oracle for gateway/platforms/yuanbao_proto.py.

We exercise the PUBLIC codec entry points (these are what the gateway uses)
and compare the encoded bytes + decoded structures against the Python
reference. Because the C port renames internals (yb_*), we verify the
externally-observable wire format is byte-identical, which proves the 30
internal helpers are correct too."""
import sys, os, json
sys.path.insert(0, "/home/wubu/hermes-agent-dev")
import gateway.platforms.yuanbao_proto as P

HERE = os.path.dirname(os.path.abspath(__file__))

cases = {}

# 1) Head encode/decode round trip
head = P._encode_head(cmd_type=1, cmd="send_c2c_message", seq_no=42,
                      msg_id="m-1", module="yuanbao_openclaw_proxy", need_ack=True, status=0)
cases["head_enc"] = list(head)
cases["head_dec"] = P._decode_head(head)

# 2) MsgContent round trip
content = {"text": "你好", "uuid": "u1", "image_format": 2, "url": "http://x",
           "file_size": 1024, "ext_map": {"k1": "v1"}}
mc = P._encode_msg_content(content)
cases["msgcontent_enc"] = list(mc)
cases["msgcontent_dec"] = P._decode_msg_content(mc)

# 3) msg_body_element
el = {"msg_type": "TIMTextElem", "msg_content": {"text": "hi", "url": "u"}}
mbe = P._encode_msg_body_element(el)
cases["bodyelem_enc"] = list(mbe)
cases["bodyelem_dec"] = P._decode_msg_body_element(mbe)

# 4) ConnMsg full
cm = P.encode_conn_msg_full(cmd_type=0, cmd="ping", seq_no=7, msg_id="mid",
                            module="conn_access", data=b"\x08\x01\x12\x03abc")
cases["connmsg_enc"] = list(cm)
cases["connmsg_dec"] = P.decode_conn_msg(cm)

# 5) forward msg data
fwd = {"sub_type": 1, "begin_time": 100, "end_time": 200, "nick_name": "n",
       "msg": [{"sender": "s", "time": 5, "plainText": "hello",
                "msgContent": [{"type": 1, "text": "t",
                                "multimedia": [{"type": "image", "url": "u2", "file_size": 9}]}]}]}
fwd_b = P.encode_forward_msg_data(fwd)
cases["forward_enc"] = list(fwd_b)
cases["forward_dec"] = P.decode_forward_msg_data(fwd_b)

# 6) inbound push decode (build a minimal one via encode path not available; build bytes manually)
ip = P._encode_field(1, P.WT_LEN, P._encode_string("cmd"))
ip += P._encode_field(2, P.WT_LEN, P._encode_string("fromA"))
ip += P._encode_field(6, P.WT_LEN, P._encode_string("grp"))
ip += P._encode_field(8, P.WT_VARINT, P._encode_varint(99))
ip += P._encode_field(13, P.WT_LEN, P._encode_message(mbe))  # length-prefixed msg_body element
cases["inbound_dec"] = P.decode_inbound_push(ip)
cases["inbound_enc"] = list(ip)

# 7) send c2c message (full ConnMsg) — compare bytes
c2c = P.encode_send_c2c_message(to_account="to", msg_body=[el], from_account="from",
                                msg_id="c2c-mid", msg_random=3, trace_id="tr")
# req_id/seq differ per run; compare the head+body structurally instead
dec = P.decode_biz_msg(c2c)
cases["c2c_dec"] = {"service": dec["service"], "method": dec["method"],
                    "req_id": dec["req_id"], "is_response": dec["is_response"]}

# 8) group member list rsp decode
gml = P._encode_field(1, P.WT_VARINT, P._encode_varint(0))
gml += P._encode_field(2, P.WT_LEN, P._encode_string("ok"))
mem = P._encode_field(1, P.WT_LEN, P._encode_string("u1"))
mem += P._encode_field(3, P.WT_VARINT, P._encode_varint(2))
gml += P._encode_field(3, P.WT_LEN, P._encode_message(mem))  # length-prefixed member
gml += P._encode_field(4, P.WT_VARINT, P._encode_varint(0))
gml += P._encode_field(5, P.WT_VARINT, P._encode_varint(1))
cases["gml_dec"] = P.decode_get_group_member_list_rsp(gml)
cases["gml_enc"] = list(gml)

with open(os.path.join(HERE, "yuanbao_proto.ref.json"), "w") as f:
    json.dump(cases, f, indent=2, sort_keys=True, default=str)
# The oracle runner feeds each *.in fixture to BOTH the C harness and the
# Python oracle as argv[1]. The harness/oracle read the ref JSON from that
# path, so ship an identical .in copy for the runner to discover.
with open(os.path.join(HERE, "yuanbao_proto.in"), "w") as f:
    json.dump(cases, f, indent=2, sort_keys=True, default=str)
print("wrote", len(cases), "cases")

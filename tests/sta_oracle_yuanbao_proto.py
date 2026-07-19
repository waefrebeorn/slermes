#!/usr/bin/env python3
"""Faithfulness oracle for port_yuanbao_proto_helpers.c.

Reads the fixture JSON (yuanbao_proto.ref.json) from argv[1]; for each covered
decode case it pulls the SAME encoded byte array and runs the LIVE
gateway.platforms.yuanbao_proto decoder, emitting compact JSON lines
{"fn":<name>,"out":<decoded json>} that the runner diffs against the C harness.

The encoded bytes come straight from the fixture (which gen_fixtures.py builds
from LIVE Python), so this is a true oracle: the C port and the live Python
decode the identical wire bytes.
"""
import sys, os, json

sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
import gateway.platforms.yuanbao_proto as P


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_yuanbao_proto.py <yuanbao_proto.ref.json>\n")
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as f:
        ref = json.load(f)

    def take_bytes(key):
        arr = ref.get(key)
        if not isinstance(arr, list):
            return None
        return bytes(int(x) & 0xFF for x in arr)

    def emit(name, decoded):
        print(json.dumps({"fn": name, "out": decoded}, separators=(",", ":"), ensure_ascii=False))

    b = take_bytes("inbound_enc")
    if b is not None:
        emit("inbound_dec", P.decode_inbound_push(b))

    b = take_bytes("msgcontent_enc")
    if b is not None:
        emit("msgcontent_dec", P._decode_msg_content(b))

    b = take_bytes("bodyelem_enc")
    if b is not None:
        emit("bodyelem_dec", P._decode_msg_body_element(b))

    b = take_bytes("gml_enc")
    if b is not None:
        emit("gml_dec", P.decode_get_group_member_list_rsp(b))

    b = take_bytes("forward_enc")
    if b is not None:
        emit("forward_dec", P.decode_forward_msg_data(b))

    return 0


if __name__ == "__main__":
    sys.exit(main())

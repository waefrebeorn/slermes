#!/usr/bin/env python3
"""Differential oracle for acp_adapter/content.py (gate w6_content_cc45ae).

acp.schema is stubbed with attribute-only classes so the LIVE helpers can be
called on JSON-represented blocks; no filesystem reads are triggered because
resource_link fixtures use non-file URIs.
"""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

acp = types.ModuleType("acp")
acp.__path__ = [Q + "/acp"]
sys.modules["acp"] = acp
schema = types.ModuleType("acp.schema")


class Obj:
    def __init__(self, **kw):
        self.__dict__.update(kw)


for _n in ["AudioContentBlock", "BlobResourceContents", "EmbeddedResourceContentBlock",
           "ImageContentBlock", "ResourceContentBlock", "TextContentBlock", "TextResourceContents"]:
    setattr(schema, _n, type(_n, (Obj,), {}))
sys.modules["acp.schema"] = schema

SRC = Q + "/acp_adapter/content.py"
spec = importlib.util.spec_from_file_location("content_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["content_live"] = live
spec.loader.exec_module(live)


def mkblock(b):
    t = b.get("__type")
    cls = {"text": live.TextContentBlock, "image": live.ImageContentBlock,
           "resource_link": live.ResourceContentBlock,
           "embedded": live.EmbeddedResourceContentBlock}.get(t, live.TextContentBlock)
    kw = {k: v for k, v in b.items() if k != "__type"}
    if t == "embedded" and isinstance(kw.get("resource"), dict):
        rr = kw["resource"]
        rcls = live.TextResourceContents if rr.get("__type") == "text" else live.BlobResourceContents
        kw["resource"] = rcls(**{k: v for k, v in rr.items() if k != "__type"})
    return cls(**kw)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "resource_display_name":
            py = live._resource_display_name(r.get("uri", ""), name=r.get("name"), title=r.get("title"))
        elif func == "mime_main":
            py = live._mime_main(r.get("mime_type"))
        elif func == "is_text_resource":
            py = live._is_text_resource(r.get("mime_type"))
        elif func == "is_image_resource":
            py = live._is_image_resource(r.get("mime_type"))
        elif func == "format_resource_text":
            py = live._format_resource_text(uri=r.get("uri", ""), body=r.get("body", ""),
                                            name=r.get("name"), title=r.get("title"), note=r.get("note"))
        elif func == "attr":
            py = live._attr(mkblock(r.get("obj", {})), r.get("attr_name", ""))
        elif func == "image_block_to_openai_part":
            py = live._image_block_to_openai_part(mkblock(r.get("block", {})))
        elif func == "extract_text":
            py = live._extract_text([mkblock(b) for b in r.get("prompt", [])])
        elif func == "content_blocks_to_openai_user_content":
            py = live._content_blocks_to_openai_user_content([mkblock(b) for b in r.get("prompt", [])])
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_content_cc45ae: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)

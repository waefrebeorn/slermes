"""twin for gateway_platforms_yuanbao_extract_text.

Traces the LIVE gateway/platforms/yuanbao.py
ExtractContentMiddleware._extract_text ladder with the same rung order as the
C harness (see harness.c header). Decisions are computed from the LIVE input
structure; the value event is the LIVE extracted text. Input: one JSON
MsgBody array per argv item.
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_yb", os.path.join(DEV, "gateway", "platforms", "yuanbao.py"))
_yb = ilu.module_from_spec(_spec)
sys.modules["wc_live_yb"] = _yb
_spec.loader.exec_module(_yb)

_EC = _yb.ExtractContentMiddleware


def trace(item):
    c = Cup("yb2_extract_text")
    body = _json.loads(item)
    is_arr = isinstance(body, list)
    c.branch(is_arr)                                                  # B1
    if is_arr:
        for elem in body:
            c.loop()
            ok = isinstance(elem, dict)
            c.branch(ok)                                              # B2
            if not ok:
                continue
            etype = elem.get("msg_type", "")
            content = elem.get("msg_content", {})
            if not isinstance(content, dict):
                content = {}
            co = lambda k: content.get(k) if k in content else None

            if etype == "TIMTextElem":                                # B3
                c.branch(True)
                text = content.get("text", "")
                c.branch(bool(text))
                continue
            c.branch(False)
            if etype == "TIMImageElem":                               # B4
                c.branch(True)
                arr = content.get("image_info_array")
                arr_list = isinstance(arr, list)
                c.branch(arr_list)                                    # B5
                pick1 = pick0 = False
                raw_url = ""
                if arr_list:
                    pick1 = len(arr) > 1 and isinstance(arr[1], dict)
                    c.branch(pick1)                                   # B6
                    if not pick1:
                        pick0 = len(arr) > 0 and isinstance(arr[0], dict)
                    c.branch(pick0)                                   # B7
                else:
                    c.branch(False); c.branch(False)
                if pick1 or pick0:
                    info = arr[1] if pick1 else arr[0]
                    u = info.get("url") if isinstance(info, dict) else None
                    raw_url = str(u or "").strip()
                has_rid = "resourceId=" in raw_url or "resourceid=" in raw_url
                c.branch(has_rid)                                     # B8
                continue
            c.branch(False)
            if etype == "TIMFileElem":                                # B9
                c.branch(True)
                furl = str(content.get("url") or "").strip()
                has_rid = "resourceId=" in furl or "resourceid=" in furl
                c.branch(has_rid)                                     # B10
                fn = co("file_name")
                if fn is None:
                    fn = co("fileName")
                if fn is None:
                    fn = content.get("filename", "")
                c.branch(bool(fn))                                    # B11
                continue
            c.branch(False)
            if etype == "TIMSoundElem":                               # B12
                c.branch(True)
                surl = str(content.get("url") or "").strip()
                c.branch("resourceId=" in surl or "resourceid=" in surl)  # B13
                continue
            c.branch(False)
            if etype == "TIMVideoFileElem":                           # B14
                c.branch(True)
                vurl = str(content.get("url") or "").strip()
                c.branch("resourceId=" in vurl or "resourceid=" in vurl)  # B15
                continue
            c.branch(False)
            if etype == "TIMCustomElem":                              # B16
                c.branch(True)
                data_val = content.get("data", "")
                c.branch(bool(data_val))                              # B17
                if data_val:
                    try:
                        custom = _json.loads(data_val)
                        parsed_ok = True
                    except (_json.JSONDecodeError, TypeError):
                        custom = None
                        parsed_ok = False
                    c.branch(parsed_ok)                               # B18
                    if parsed_ok:
                        c.branch(isinstance(custom, dict))            # B19
                        if isinstance(custom, dict):
                            ct = custom.get("elem_type")
                            c.branch(ct == 1002)                      # B20
                            if ct != 1002:
                                c.branch(ct == 1010)                  # B21
                                if ct != 1010:
                                    c.branch(ct == 1007)              # B22
                                    if ct == 1007:
                                        cont = custom.get("content")
                                        lk = None
                                        if cont:
                                            try:
                                                p = _json.loads(cont)
                                                lk = p.get("link") if isinstance(p, dict) else None
                                            except (_json.JSONDecodeError, TypeError):
                                                lk = None
                                        c.branch(isinstance(lk, str) and bool(lk))  # B23
                                    else:
                                        c.branch(ct == 1009)          # B24
                    else:
                        c.branch(False)                               # B19 (parse failed)
                continue
            c.branch(False)
            if etype == "TIMFaceElem":                                # B25
                c.branch(True)
                raw_data = content.get("data", "")
                c.branch(bool(raw_data))                              # B26
                named = False
                if raw_data:
                    try:
                        fd = _json.loads(raw_data)
                        n = fd.get("name") if isinstance(fd, dict) else None
                        if isinstance(n, str) and n.strip():
                            named = True
                    except (_json.JSONDecodeError, TypeError, AttributeError):
                        pass
                c.branch(named)                                       # B27
                continue
            c.branch(False)
            c.branch(bool(etype))                                     # B28

    live = _EC._extract_text(body)
    c.valstr(live)
    return c.finalize()


def run(inputs):
    return [trace(i) for i in inputs]

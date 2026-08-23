"""twin for gateway_platforms_api_server_normalize_multimodal_content.

Traces the LIVE gateway/platforms/api_server.py::_normalize_multimodal_content
ladder with the same rung order as the C harness (see harness.c header for the
rung list). The value event is derived from the LIVE result (or ValueError)
with the same canonical encoding the C harness derives from the REAL port's
JSON output. Input: one JSON content value per argv item.
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_aps", os.path.join(DEV, "gateway", "platforms", "api_server.py"))
_aps = ilu.module_from_spec(_spec)
sys.modules["wc_live_aps"] = _aps
_spec.loader.exec_module(_aps)

_live_fn = _aps._normalize_multimodal_content
TEXT_TYPES = ("text", "input_text", "output_text")
IMAGE_TYPES = ("image_url", "input_image")
FILE_TYPES = ("file", "input_file")
MAX_TEXT = 65536
MAX_LIST = 1000


def _canon(res):
    if isinstance(res, tuple):          # ("ERR", message)
        return "ERR|" + res[1]
    if res is None or isinstance(res, str):
        return "STR|" + (res or "")
    parts = []
    for p in res:
        ty = p.get("type") if isinstance(p, dict) else None
        if ty == "text":
            parts.append("t:" + str(p.get("text")))
        else:
            iu = p.get("image_url") if isinstance(p, dict) else None
            if isinstance(iu, dict):
                d = iu.get("detail")
                ds = d if isinstance(d, str) else "?"
                if d is None:
                    ds = "-"
                parts.append("i:%s|%s" % (iu.get("url"), ds))
            else:
                parts.append("i:%s|?" % (iu,))
    return "PARTS|" + ";".join(parts)


def trace(item):
    c = Cup("aps_normalize_multimodal_content")
    try:
        live = _live_fn(_json.loads(item))
        err = None
    except ValueError as e:
        live = None
        err = str(e)

    content = _json.loads(item)
    any_error = err is not None
    nparts = 0
    all_text = True

    c.branch(content is None)                                     # B1
    c.branch(isinstance(content, str))                            # B2
    if isinstance(content, list):
        c.branch(True)                                            # B3
        items = content[:MAX_LIST] if len(content) > MAX_LIST else content
        for part in items:
            c.loop()
            if isinstance(part, str):                             # B4
                c.branch(True)
                c.branch(bool(part))                              # B5
                nparts += 1
                continue
            c.branch(False)
            if not isinstance(part, dict):                        # B6
                c.branch(False)
                continue
            c.branch(True)
            ptype = (str(part.get("type") or "").strip().lower())
            if ptype in TEXT_TYPES:                               # B7
                c.branch(True)
                text = part.get("text")
                missing = text is None
                c.branch(missing)                                 # B8
                truthy = bool(text) if not missing else False
                c.branch(truthy)                                  # B9
                if truthy:
                    nparts += 1
                continue
            c.branch(False)
            if ptype in IMAGE_TYPES:                              # B10
                c.branch(True)
                detail = part.get("detail")
                image_ref = part.get("image_url")
                c.branch(isinstance(image_ref, dict))             # B11
                if isinstance(image_ref, dict):
                    url_value = image_ref.get("url")
                    detail = image_ref.get("detail", detail)
                else:
                    url_value = image_ref
                url_bad = not isinstance(url_value, str) or not url_value.strip()
                c.branch(url_bad)                                 # B12
                if url_bad:
                    break
                u = url_value.strip()
                lowered = u.lower()
                is_data = lowered.startswith("data:")
                c.branch(is_data)                                 # B13
                if is_data:
                    okimg = lowered.startswith("data:image/") and "," in u
                    c.branch(okimg)                               # B14
                    if not okimg:
                        break
                else:
                    is_http = lowered.startswith("http://") or lowered.startswith("https://")
                    c.branch(is_http)                             # B15
                    if not is_http:
                        break
                have_detail = detail is not None
                c.branch(have_detail)                             # B16
                if have_detail:
                    bad = (not isinstance(detail, str)) or not detail.strip()
                    c.branch(bad)                                 # B17
                    if bad:
                        break
                nparts += 1
                all_text = False
                continue
            c.branch(False)
            if ptype in FILE_TYPES:                               # B18
                c.branch(True)
                break
            c.branch(False)
            c.branch(True)                                        # B19 unknown
            break
        c.branch(any_error)                                       # B20
        if not any_error:
            c.branch(nparts == 0)                                 # B21
            if nparts:
                c.branch(all_text)                                # B22
    else:
        c.branch(False)

    if err is not None:
        val = "ERR|" + err
    elif isinstance(live, str):
        val = "STR|" + live
    else:
        val = _canon(live)
    c.valstr(val)
    return c.finalize()


def run(inputs):
    return [trace(i) for i in inputs]

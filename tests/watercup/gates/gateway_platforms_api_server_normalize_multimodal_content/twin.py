"""twin for gateway/platforms/api_server.py:_normalize_multimodal_content.

Input is a JSON content value (same text the C harness json_parses). The twin
parses it too, calls the real fn, and renders the SAME canon the harness derives
from the C port's JSON output:
  error  -> "ERR|<msg>"     (C: {"error": ...}; python: ValueError("code:message"))
  string -> "STR|<value>"   (C: json string; python: str / "" )
  list   -> "PARTS|t:<text>;i:<url>|<detail-or->"
"""
from watercup_twin_runtime import TwinBase
import json as _j

_INVALID = "invalid_content_part:Content must be valid JSON."


class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_normalize_multimodal_content"
    C_FN = "aps_normalize_multimodal_content"

    def call(self, inp, inputs, idx=0):
        s = inp if inp is not None else ""
        try:
            content = _j.loads(s) if s != "" else None
        except Exception:
            return ("ERR", _INVALID)
        try:
            return self._fn(content)
        except ValueError as e:
            return ("ERR", str(e))
        except Exception as e:
            return ("ERR", "%s:%s" % (type(e).__name__, e))

    def norm(self, c, result):
        if isinstance(result, tuple) and len(result) == 2 and result[0] == "ERR":
            c.valstr("ERR|" + result[1]); return
        if isinstance(result, list):
            out = []
            for p in result:
                t = p.get("type") if isinstance(p, dict) else None
                if t == "text":
                    out.append("t:" + str((p.get("text") if isinstance(p, dict) else "") or "?"))
                else:
                    iu = p.get("image_url") if isinstance(p, dict) else None
                    u = iu.get("url") if isinstance(iu, dict) else None
                    d = iu.get("detail") if isinstance(iu, dict) else None
                    out.append("i:" + str(u or "?") + "|" + (str(d) if d is not None else "-"))
            c.valstr("PARTS|" + ";".join(out)); return
        if isinstance(result, str):
            c.valstr("STR|" + result); return
        c.valstr("STR|" + ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

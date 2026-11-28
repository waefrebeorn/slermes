"""twin for gateway/platforms/api_server.py:_origin_allowed (real receiver).

The live fn is a method: `_origin_allowed(self, origin) -> bool` reading
`self._cors_origins`. The C port is `aps_origin_allowed(origin, cors_json)`.
Each gate input is a scenario "origin|||cors_origins_json": build a receiver
with _cors_origins = json.loads(cors_json) and call the real method.
"""
from watercup_twin_runtime import TwinBase
import json as _j


class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "APIServerAdapter._origin_allowed"
    C_FN = "aps_origin_allowed"

    def call(self, inp, inputs, idx=0):
        s = str(inp)
        if "|||" in s:
            origin, corsjson = s.split("|||", 1)
        else:
            origin, corsjson = s, ""
        cors = []
        if corsjson.strip():
            try:
                v = _j.loads(corsjson)
                if isinstance(v, list):
                    cors = v
            except Exception:
                cors = []
        cls = getattr(self._mod, "APIServerAdapter")
        rcv = type("_R", (), {"_cors_origins": cors})()
        return cls._origin_allowed(rcv, origin)

    def norm(self, c, result):
        # Align with the harness's WCUP_VALINT(result) -> "1"/"0".
        c.valstr("1" if result else "0")


_t = _Twin()
trace = _t.trace
run = _t.run

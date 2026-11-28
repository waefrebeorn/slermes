"""twin: _content_to_blocks(content, redact) -> list[dict]."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/trace_upload.py"; PY_FN = "_content_to_blocks"
    C_FN = "agent_trace_upload_u_content_to_blocks"
    def call(self, inp, inputs, idx=0):
        d = _j.loads(inp)
        return self._fn(d.get("content"), bool(d.get("redact", False)))
    def norm(self, c, result):
        c.valstr(_j.dumps(list(result), separators=(",", ":")))
_t = _Twin(); trace = _t.trace; run = _t.run

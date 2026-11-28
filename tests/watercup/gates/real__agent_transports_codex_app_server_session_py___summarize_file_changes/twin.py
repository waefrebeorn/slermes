"""twin: _summarize_file_changes(raw_changes) -> one-line summary."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_app_server_session.py"; PY_FN = "_summarize_file_changes"
    C_FN = "agent_codex_app_server_session_u_summarize_file_changes"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run

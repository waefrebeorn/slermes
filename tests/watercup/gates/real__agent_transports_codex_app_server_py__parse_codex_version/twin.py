"""twin for agent/transports/codex_app_server.py:parse_codex_version (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex_app_server.py"
    PY_FN = "parse_codex_version"
    C_FN = "parse_codex_version"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

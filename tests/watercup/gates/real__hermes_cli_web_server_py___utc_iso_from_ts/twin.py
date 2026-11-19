"""twin for hermes_cli/web_server.py:_utc_iso_from_ts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_utc_iso_from_ts"
    C_FN = "_utc_iso_from_ts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

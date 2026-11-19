"""twin for hermes_cli/plugins.py:_refresh_secret_sources_after_discovery (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_refresh_secret_sources_after_discovery"
    C_FN = "_refresh_secret_sources_after_discovery"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

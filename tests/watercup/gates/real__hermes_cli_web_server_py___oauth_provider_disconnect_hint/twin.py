"""twin for hermes_cli/web_server.py:_oauth_provider_disconnect_hint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_oauth_provider_disconnect_hint"
    C_FN = "_oauth_provider_disconnect_hint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

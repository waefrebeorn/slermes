"""twin for hermes_cli/web_server.py:_models_from_custom_endpoint_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_models_from_custom_endpoint_entry"
    C_FN = "_models_from_custom_endpoint_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

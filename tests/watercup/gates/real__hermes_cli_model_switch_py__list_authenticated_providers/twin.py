"""twin for hermes_cli/model_switch.py:list_authenticated_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_switch.py"
    PY_FN = "list_authenticated_providers"
    C_FN = "list_authenticated_providers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

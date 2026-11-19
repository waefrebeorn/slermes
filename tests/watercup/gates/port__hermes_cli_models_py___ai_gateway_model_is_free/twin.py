"""twin for hermes_cli/models.py:_ai_gateway_model_is_free (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_ai_gateway_model_is_free"
    C_FN = "_ai_gateway_model_is_free"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

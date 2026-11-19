"""twin for hermes_cli/model_normalize.py:_strip_matching_provider_prefix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_normalize.py"
    PY_FN = "_strip_matching_provider_prefix"
    C_FN = "_strip_matching_provider_prefix"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

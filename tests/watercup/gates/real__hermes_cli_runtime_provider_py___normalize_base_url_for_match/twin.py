"""twin for hermes_cli/runtime_provider.py:_normalize_base_url_for_match (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/runtime_provider.py"
    PY_FN = "_normalize_base_url_for_match"
    C_FN = "_normalize_base_url_for_match"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

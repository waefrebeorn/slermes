"""twin for hermes_cli/runtime_provider.py:_detect_api_mode_for_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/runtime_provider.py"
    PY_FN = "_detect_api_mode_for_url"
    C_FN = "_detect_api_mode_for_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

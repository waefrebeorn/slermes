"""twin for hermes_cli/proxy/adapters/xai.py:_credential_from_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy/adapters/xai.py"
    PY_FN = "_credential_from_entry"
    C_FN = "_credential_from_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

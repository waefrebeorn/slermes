"""twin for tools/env_passthrough.py:_is_hermes_provider_credential (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/env_passthrough.py"
    PY_FN = "_is_hermes_provider_credential"
    C_FN = "_is_hermes_provider_credential"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

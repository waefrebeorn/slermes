"""twin for hermes_cli/moa_config.py:validate_moa_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/moa_config.py"
    PY_FN = "validate_moa_payload"
    C_FN = "validate_moa_payload"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

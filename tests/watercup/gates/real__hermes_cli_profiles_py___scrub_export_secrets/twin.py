"""twin for hermes_cli/profiles.py:_scrub_export_secrets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "_scrub_export_secrets"
    C_FN = "_scrub_export_secrets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

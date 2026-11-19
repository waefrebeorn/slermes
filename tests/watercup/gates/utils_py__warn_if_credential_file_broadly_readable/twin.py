"""twin for utils.py:warn_if_credential_file_broadly_readable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "utils.py"
    PY_FN = "warn_if_credential_file_broadly_readable"
    C_FN = "util_warn_if_credential_file_broadly_readable"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

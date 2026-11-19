"""twin for hermes_cli/profiles.py:_read_distribution_meta (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "_read_distribution_meta"
    C_FN = "profile_read_distribution_meta"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

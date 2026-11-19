"""twin for hermes_cli/auth.py:_heal_forked_single_use_oauth_grants (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_heal_forked_single_use_oauth_grants"
    C_FN = "_heal_forked_single_use_oauth_grants"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

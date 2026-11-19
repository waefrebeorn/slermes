"""twin for agent/secret_sources/onepassword.py:apply_onepassword_secrets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/onepassword.py"
    PY_FN = "apply_onepassword_secrets"
    C_FN = "opw_apply_onepassword_secrets"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

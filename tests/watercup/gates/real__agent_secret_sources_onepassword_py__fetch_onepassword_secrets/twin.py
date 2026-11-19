"""twin for agent/secret_sources/onepassword.py:fetch_onepassword_secrets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/onepassword.py"
    PY_FN = "fetch_onepassword_secrets"
    C_FN = "fetch_onepassword_secrets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

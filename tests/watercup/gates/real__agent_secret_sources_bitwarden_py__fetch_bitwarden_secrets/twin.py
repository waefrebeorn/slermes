"""twin for agent/secret_sources/bitwarden.py:fetch_bitwarden_secrets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"
    PY_FN = "fetch_bitwarden_secrets"
    C_FN = "fetch_bitwarden_secrets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

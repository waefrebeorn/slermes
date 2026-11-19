"""twin for agent/secret_sources/bitwarden.py:apply_bitwarden_secrets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"
    PY_FN = "apply_bitwarden_secrets"
    C_FN = "bw_apply_bitwarden_secrets"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

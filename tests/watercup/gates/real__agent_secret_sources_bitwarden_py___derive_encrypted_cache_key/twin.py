"""twin for agent/secret_sources/bitwarden.py:_derive_encrypted_cache_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"
    PY_FN = "_derive_encrypted_cache_key"
    C_FN = "_derive_encrypted_cache_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

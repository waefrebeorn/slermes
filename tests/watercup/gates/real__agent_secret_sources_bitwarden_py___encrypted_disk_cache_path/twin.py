"""twin for agent/secret_sources/bitwarden.py:_encrypted_disk_cache_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/bitwarden.py"
    PY_FN = "_encrypted_disk_cache_path"
    C_FN = "_encrypted_disk_cache_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

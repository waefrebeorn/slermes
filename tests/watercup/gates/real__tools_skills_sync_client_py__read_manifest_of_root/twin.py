"""twin for tools/skills_sync_client.py:read_manifest_of_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync_client.py"
    PY_FN = "read_manifest_of_root"
    C_FN = "read_manifest_of_root"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

"""twin for tools/skills_sync_client.py:sync_org_auto_propose (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync_client.py"
    PY_FN = "sync_org_auto_propose"
    C_FN = "skills_sync_org_auto_propose"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

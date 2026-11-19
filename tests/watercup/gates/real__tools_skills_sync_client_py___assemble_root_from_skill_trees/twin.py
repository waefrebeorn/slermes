"""twin for tools/skills_sync_client.py:_assemble_root_from_skill_trees (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync_client.py"
    PY_FN = "_assemble_root_from_skill_trees"
    C_FN = "_assemble_root_from_skill_trees"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

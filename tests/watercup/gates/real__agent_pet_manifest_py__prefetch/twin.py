"""twin for agent/pet/manifest.py:prefetch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/manifest.py"
    PY_FN = "prefetch"
    C_FN = "pet_prefetch_manifest"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

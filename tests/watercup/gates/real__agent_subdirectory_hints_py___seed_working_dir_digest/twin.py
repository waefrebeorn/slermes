"""twin for agent/subdirectory_hints.py:_seed_working_dir_digest (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subdirectory_hints.py"
    PY_FN = "_seed_working_dir_digest"
    C_FN = "sha256_to_hex"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run

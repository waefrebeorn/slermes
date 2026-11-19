"""twin for tools/blueprints.py:blueprint_to_job_spec (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/blueprints.py"
    PY_FN = "blueprint_to_job_spec"
    C_FN = "blueprint_to_job_spec"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run

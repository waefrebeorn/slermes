"""twin for hermes_cli/profiles.py:has_bundled_skills_opt_out (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profiles.py"
    PY_FN = "has_bundled_skills_opt_out"
    C_FN = "profile_has_bundled_skills_opt_out"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run

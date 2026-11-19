"""twin for hermes_cli/skills_hub.py:_derive_category_from_install_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skills_hub.py"
    PY_FN = "_derive_category_from_install_path"
    C_FN = "hermes_cli_skills_hub_u_derive_category_from_install_path"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

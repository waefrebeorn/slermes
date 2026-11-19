"""twin for hermes_cli/projects_db.py:get_discovery_policy_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/projects_db.py"
    PY_FN = "get_discovery_policy_key"
    C_FN = "hermes_cli_projects_db_get_discovery_policy_key"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run

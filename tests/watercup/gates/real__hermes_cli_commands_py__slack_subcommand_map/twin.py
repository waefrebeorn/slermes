"""twin for hermes_cli/commands.py:slack_subcommand_map (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/commands.py"
    PY_FN = "slack_subcommand_map"
    C_FN = "cmd_slack_subcommand_map"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run

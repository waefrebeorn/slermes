"""twin for agent/account_usage.py:redeemed (thin: refers to shared runtime)."""
from watercup_twin_runtime import make_twin

_t = make_twin(
    "agent/account_usage.py", "redeemed", "agent_account_usage_redeemed",
    'str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result))',
)
trace = _t.trace
run = _t.run

"""twin for agent_billing_usage_usage_model_from_account.

Traces the LIVE agent/billing_usage.py::usage_model_from_account with the
SAME rung order as the C harness:
  B1 no-object/not-logged-in / B2 paid-is-false / B3 free /
  B4 low-balance / B5 plan bar / B6 topup bar /
  VSTR(status|sub_rem@p|topup_rem@p|total@p|
       planbar rem@p/tot@p/spent@p|topbar rem@p/tot@p|plan).
The JSON input is inflated into a SimpleNamespace account object so the LIVE
function consumes it exactly as in production.
"""
import os, sys, json, importlib.util as ilu
from types import SimpleNamespace as NS

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_billing_usage", os.path.join(DEV, "agent", "billing_usage.py"))
_bu = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_billing_usage"] = _bu
try:
    _spec.loader.exec_module(_bu)
    _live_fn = _bu.usage_model_from_account
except Exception:
    raise


def _maybe_ns(v):
    return NS(**v) if isinstance(v, dict) else None


def inflate(account_json):
    ai = json.loads(account_json)
    if not isinstance(ai, dict):
        return None
    acc = NS(**ai)
    acc.paid_service_access_info = NS(**ai["paid_service_access_info"]) \
        if isinstance(ai.get("paid_service_access_info"), dict) else \
        ai.get("paid_service_access_info")
    acc.subscription = NS(**ai["subscription"]) \
        if isinstance(ai.get("subscription"), dict) else ai.get("subscription")
    return acc


def fmt_d(v, present):
    return "%.17g" % v if present else "-"


def trace(account_json):
    c = Cup("usage_model_from_account")
    try:
        acc = inflate(account_json)
        logged_in = bool(acc is not None and getattr(acc, "logged_in", False))
    except (json.JSONDecodeError, TypeError, AttributeError):
        acc, logged_in = None, False

    c.branch(not logged_in)                                     # B1
    if not logged_in or acc is None:
        m = _live_fn(acc)
    else:
        paid_is_false = (getattr(acc, "paid_service_access", "missing") is False)
        c.branch(paid_is_false)                                 # B2
        access = getattr(acc, "paid_service_access_info", None)
        sub = getattr(acc, "subscription", None)

        def fin(o, k):
            raw = getattr(o, k, None) if o is not None else None
            if raw is None:
                return None
            try:
                f = float(raw)
            except (TypeError, ValueError):
                return None
            return f  # LIVE _finite also requires >0? see module; mirror via result anyway

        # evaluate ladder conditions from the inflated object
        sub_r = fin(access, "subscription_credits_remaining")
        top_r = fin(access, "purchased_credits_remaining")
        tot_u = fin(access, "total_usable_credits")
        monthly = fin(sub, "monthly_credits")
        plan = getattr(sub, "plan", None) if sub is not None else None
        has_subscription = bool(plan) or (monthly is not None and monthly > 0)
        parts = [v for v in (sub_r, top_r) if v is not None]
        total_spendable = tot_u if tot_u is not None else (sum(parts) if parts else None)
        free_case = (not has_subscription and not (top_r is not None and top_r > 0))
        if not paid_is_false:
            c.branch(free_case)                                 # B3
            if not free_case:
                c.branch(total_spendable is not None and total_spendable < 5.0)  # B4
            c.branch(monthly is not None and monthly > 0 and sub_r is not None)  # B5
            c.branch(top_r is not None and top_r > 0)           # B6
        m = _live_fn(acc)

    if not m.available:
        c.valstr("unavailable")
        return c.finalize()

    def d(v):
        return "%.17g" % v if v is not None else "-"
    pb = m.plan_bar
    tb = m.topup_bar
    label = "%s|%s|%s|%s|%s/%s/%s|%s/%s|%s" % (
        m.status,
        d(m.subscription_remaining_usd), d(m.topup_remaining_usd),
        d(m.total_spendable_usd),
        d(pb.remaining_usd) if pb else "-", d(pb.total_usd) if pb else "-",
        d(pb.spent_usd) if pb else "-",
        d(tb.remaining_usd) if tb else "-", d(tb.total_usd) if tb else "-",
        m.plan_name if m.plan_name else "-")
    c.valstr(label)
    return c.finalize()


def run(inputs):
    return [trace(t) for t in inputs]

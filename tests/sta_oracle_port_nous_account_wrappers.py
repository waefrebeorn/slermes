"""AUTO-GENERATED integration oracle for port_nous_account_wrappers (gen_integration_oracle.py)."""
import os, sys, json, importlib.util

MODS = {}
def _load(rel):
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path: sys.path.insert(0, _repo)
    for base in sys.path:
        cand = os.path.join(base, rel)
        try:
            spec = importlib.util.spec_from_file_location('live_' + rel.replace('/', '_').replace('.', '_'), cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    return None
MODS['hermes_cli.nous_account'] = _load('hermes_cli/nous_account.py')

DISPATCH = {
    'nous_tool_gateway_entitled': ('hermes_cli.nous_account', 'tool_gateway_entitled'),
    'nous_tool_gateway_entitled_for': ('hermes_cli.nous_account', 'tool_gateway_entitled_for'),
    'nous_nous_portal_billing_url': ('hermes_cli.nous_account', 'nous_portal_billing_url'),
    'nous_nous_portal_topup_url': ('hermes_cli.nous_account', 'nous_portal_topup_url'),
    'nous_format_nous_portal_entitlement_message': ('hermes_cli.nous_account', 'format_nous_portal_entitlement_message'),
    'nous_u_no_paid_access_message': ('hermes_cli.nous_account', '_no_paid_access_message'),
    'nous_u_credit_detail': ('hermes_cli.nous_account', '_credit_detail'),
    'nous_reset_nous_portal_account_info_cache': ('hermes_cli.nous_account', 'reset_nous_portal_account_info_cache'),
    'nous_u_fresh_account_info': ('hermes_cli.nous_account', '_fresh_account_info'),
    'nous_u_info_from_inference_key_pool': ('hermes_cli.nous_account', '_info_from_inference_key_pool'),
    'nous_u_info_from_oauth_pool': ('hermes_cli.nous_account', '_info_from_oauth_pool'),
    'nous_u_select_nous_pool_entry': ('hermes_cli.nous_account', '_select_nous_pool_entry'),
    'nous_u_pool_entry_is_portal_oauth': ('hermes_cli.nous_account', '_pool_entry_is_portal_oauth'),
    'nous_u_fetch_nous_account_info': ('hermes_cli.nous_account', '_fetch_nous_account_info'),
    'nous_u_info_from_valid_jwt': ('hermes_cli.nous_account', '_info_from_valid_jwt'),
    'nous_u_info_from_account_payload': ('hermes_cli.nous_account', '_info_from_account_payload'),
    'nous_u_tool_access_from_value': ('hermes_cli.nous_account', '_tool_access_from_value'),
    'nous_u_subscription_from_payload': ('hermes_cli.nous_account', '_subscription_from_payload'),
    'nous_u_paid_service_access_from_payload': ('hermes_cli.nous_account', '_paid_service_access_from_payload'),
    'nous_u_error_info': ('hermes_cli.nous_account', '_error_info'),
    'nous_u_portal_base_url': ('hermes_cli.nous_account', '_portal_base_url'),
    'nous_u_cache_key': ('hermes_cli.nous_account', '_cache_key'),
    'nous_u_parse_iso_timestamp': ('hermes_cli.nous_account', '_parse_iso_timestamp'),
    'nous_u_coerce_str': ('hermes_cli.nous_account', '_coerce_str'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_nous_account_wrappers.py <cases.json>\n"); return 2
    with open(sys.argv[1], 'r', encoding='utf-8') as f: cases = json.load(f)
    for c in cases:
        op = c.get('op'); value = c.get('value', '')
        d = DISPATCH.get(op)
        if not d: sys.stdout.write(json.dumps({'fn':op}, separators=(',',':')) + '\n'); continue
        pymod, pyfn = d
        mod = MODS.get(pymod)
        try:
            out = getattr(mod, pyfn)(value) if mod else None
        except Exception as e:
            out = 'PYERR:' + str(e)
        if isinstance(out, bool): out = bool(out)
        elif isinstance(out, (int, float)) and not isinstance(out, bool): out = int(out)
        elif out is None: out = ''
        else: out = str(out)
        sys.stdout.write(json.dumps({'fn':op,'out':out}, ensure_ascii=True, separators=(',',':')) + '\n')
    return 0

if __name__ == '__main__':
    sys.exit(main())

"""AUTO-GENERATED integration oracle for port_other_remaining_wrappers (gen_integration_oracle.py)."""
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
MODS['cron.executions'] = _load('cron/executions.py')
MODS['cron.jobs'] = _load('cron/jobs.py')
MODS['cron.scheduler'] = _load('cron/scheduler.py')
MODS['cron.scheduler_provider'] = _load('cron/scheduler_provider.py')

DISPATCH = {
    'cron_executions_u_connect': ('cron.executions', '_connect'),
    'cron_executions_u_initialize_schema': ('cron.executions', '_initialize_schema'),
    'cron_executions_u_transaction': ('cron.executions', '_transaction'),
    'cron_executions_u_process_start_time': ('cron.executions', '_process_start_time'),
    'cron_executions_u_owner_is_live': ('cron.executions', '_owner_is_live'),
    'cron_executions_u_prune_unlocked': ('cron.executions', '_prune_unlocked'),
    'cron_executions_create_execution': ('cron.executions', 'create_execution'),
    'cron_executions_mark_execution_running': ('cron.executions', 'mark_execution_running'),
    'cron_executions_finish_execution': ('cron.executions', 'finish_execution'),
    'cron_executions_recover_interrupted_executions': ('cron.executions', 'recover_interrupted_executions'),
    'cron_executions_list_executions': ('cron.executions', 'list_executions'),
    'cron_executions_latest_execution': ('cron.executions', 'latest_execution'),
    'cron_executions_latest_executions': ('cron.executions', 'latest_executions'),
    'cron_jobs_u_current_cron_store': ('cron.jobs', '_current_cron_store'),
    'cron_jobs_use_cron_store': ('cron.jobs', 'use_cron_store'),
    'cron_jobs_get_cron_output_dir': ('cron.jobs', 'get_cron_output_dir'),
    'cron_jobs_u_oneshot_run_claim_ttl_seconds': ('cron.jobs', '_oneshot_run_claim_ttl_seconds'),
    'cron_jobs_u_job_running_in_this_process': ('cron.jobs', '_job_running_in_this_process'),
    'cron_jobs_u_preserve_file_ownership': ('cron.jobs', '_preserve_file_ownership'),
    'cron_jobs_record_ticker_error': ('cron.jobs', 'record_ticker_error'),
    'cron_jobs_clear_ticker_error': ('cron.jobs', 'clear_ticker_error'),
    'cron_jobs_get_ticker_last_error': ('cron.jobs', 'get_ticker_last_error'),
    'cron_scheduler_u_windows_cron_python_invocation': ('cron.scheduler', '_windows_cron_python_invocation'),
    'cron_scheduler_u_teardown_cron_agent': ('cron.scheduler', '_teardown_cron_agent'),
    'cron_scheduler_provider_recover_interrupted': ('cron.scheduler_provider', 'recover_interrupted'),
    'cron_scheduler_provider_u_start_multiplex': ('cron.scheduler_provider', '_start_multiplex'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_other_remaining_wrappers.py <cases.json>\n"); return 2
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

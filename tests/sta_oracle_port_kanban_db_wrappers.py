"""AUTO-GENERATED integration oracle for port_kanban_db_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.kanban_db'] = _load('hermes_cli/kanban_db.py')

DISPATCH = {
    'kdbport_u_assert_not_delegated_child_mutation': ('hermes_cli.kanban_db', '_assert_not_delegated_child_mutation'),
    'kdbport_scoped_current_board': ('hermes_cli.kanban_db', 'scoped_current_board'),
    'kdbport_from_row': ('hermes_cli.kanban_db', 'from_row'),
    'kdbport_from_row_2': ('hermes_cli.kanban_db', 'from_row'),
    'kdbport_u_sqlite_connect': ('hermes_cli.kanban_db', '_sqlite_connect'),
    'kdbport_u_maybe_checkpoint_wal': ('hermes_cli.kanban_db', '_maybe_checkpoint_wal'),
    'kdbport_u_prune_corrupt_backups': ('hermes_cli.kanban_db', '_prune_corrupt_backups'),
    'kdbport_u_integrity_messages_ok': ('hermes_cli.kanban_db', '_integrity_messages_ok'),
    'kdbport_u_run_integrity_check': ('hermes_cli.kanban_db', '_run_integrity_check'),
    'kdbport_u_repairable_index_names': ('hermes_cli.kanban_db', '_repairable_index_names'),
    'kdbport_u_attempt_index_reindex_repair': ('hermes_cli.kanban_db', '_attempt_index_reindex_repair'),
    'kdbport_repair_db': ('hermes_cli.kanban_db', 'repair_db'),
    'kdbport_u_migrate_add_optional_columns': ('hermes_cli.kanban_db', '_migrate_add_optional_columns'),
    'kdbport_set_model_override': ('hermes_cli.kanban_db', 'set_model_override'),
    'kdbport_u_safe_attachment_name': ('hermes_cli.kanban_db', '_safe_attachment_name'),
    'kdbport_u_collision_free_path': ('hermes_cli.kanban_db', '_collision_free_path'),
    'kdbport_store_attachment_bytes': ('hermes_cli.kanban_db', 'store_attachment_bytes'),
    'kdbport_u_merge_completion_prose_artifacts': ('hermes_cli.kanban_db', '_merge_completion_prose_artifacts'),
    'kdbport_u_persist_scratch_completion_artifacts': ('hermes_cli.kanban_db', '_persist_scratch_completion_artifacts'),
    'kdbport_u_insert_completion_attachment': ('hermes_cli.kanban_db', '_insert_completion_attachment'),
    'kdbport_u_unique_attachment_path': ('hermes_cli.kanban_db', '_unique_attachment_path'),
    'kdbport_u_managed_scratch_path_info': ('hermes_cli.kanban_db', '_managed_scratch_path_info'),
    'kdbport_decompose_triage_task': ('hermes_cli.kanban_db', 'decompose_triage_task'),
    'kdbport_u_protocol_violation_streak': ('hermes_cli.kanban_db', '_protocol_violation_streak'),
    'kdbport_list_runs': ('hermes_cli.kanban_db', 'list_runs'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_kanban_db_wrappers.py <cases.json>\n"); return 2
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

#!/usr/bin/env python3
"""Generate tests/fixtures/api_server_runs_flow.jsonl for the run-lifecycle FLOW
port (src/gateway/platforms/api_server_runs_flow.c).

Case shape: {"func": <harness name>, "in": {...flat args + seam scripts...}}.
The C harness (t_port_api_server_runs_flow.c) and the live oracle
(oracle_api_server_runs_flow.py) consume the identical "in" keys: seam scripts
(clock number/sequence, redact_ret, room_token, claims_ret/claims_raises,
profile, api_key_ret, request_set_raises, retention_until, check_auth_ret,
pid_exists_ret/raises, start_time_ret/raises, fp_match_ret/raises,
projection_ret, store_record, store_update_raises, stored_ret) plus the flow
args (result, raw_input, kw, fields, request, claims, state, request_state,
body, record, run_id, name, status, permission, outcome, owner_pid,
owner_started, gateway_session_key, replayed, tables).

Every ported function gets >= 5 cases. Error paths (KeyError / ValueError /
AttributeError / TypeError / RuntimeError), the arg-binding TypeError on
_run_event, the non-dict .get AttributeError on _run_user_message, present-null
status hydration, and BaseException fail-closed escapes are covered.

Domain notes (kept faithful to the C port header):
  * integral JSON numbers read as ints on both sides; integral-FLOAT literals
    (e.g. 5.0) are excluded from the .get-AttributeError and json.dumps-preview
    domains because live json.loads keeps them float while libjson does not.
  * float()/int() coercion fixtures use clean decimal strings/numbers.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "api_server_runs_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


# ── base corpus (run-lifecycle flows) ──
add('terminal_run_status', **{'result': {}})
add('terminal_run_status', **{'result': {'interrupted': True}})
add('terminal_run_status', **{'result': {'failed': True}})
add('terminal_run_status', **{'result': {'partial': True}})
add('terminal_run_status', **{'result': {'completed': False}})
add('terminal_run_status', **{'result': {'completed': 0}})
add('terminal_run_status', **{'result': {'failed': True, 'turn_exit_reason': 'budget', 'pending_steer': 'go left'}})
add('terminal_run_status', **{'result': {'interrupted': True, 'turn_exit_reason': 'stop', 'completed': True}})
add('terminal_run_status', **{'result': {'turn_exit_reason': 'x'}})
add('terminal_run_status', **{'result': {'pending_steer': ''}})
add('run_user_message', **{'raw_input': None})
add('run_user_message', **{'raw_input': ''})
add('run_user_message', **{'raw_input': []})
add('run_user_message', **{'raw_input': {}})
add('run_user_message', **{'raw_input': 0})
add('run_user_message', **{'raw_input': 'hello'})
add('run_user_message', **{'raw_input': [{'content': 'hi'}]})
add('run_user_message', **{'raw_input': [{'role': 'user'}]})
add('run_user_message', **{'raw_input': [5]})
add('run_user_message', **{'raw_input': [{'content': ''}]})
add('run_user_message', **{'raw_input': [{'content': 'a'}, {'content': 'b'}]})
add('run_user_message', **{'raw_input': {'a': 1}})
add('tool_completed_preview', **{'result': None})
add('tool_completed_preview', **{'result': 'secret text', 'redact_ret': '[REDACTED]'})
add('tool_completed_preview', **{'result': {'a': 1, 'b': [True, None, 'x']}})
add('tool_completed_preview', **{'result': 'abc', 'redact_ret': 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'})
add('tool_completed_preview', **{'result': 'abc', 'redact_ret': 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'})
add('tool_completed_preview', **{'result': 'abc', 'redact_ret': 'cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc'})
add('tool_completed_preview', **{'result': 'abc', 'redact_ret': 'éééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééééé'})
add('tool_completed_preview', **{'result': 42})
add('tool_completed_preview', **{'result': 'x', 'redact_ret': ''})
add('run_event', **{'run_id': 'r1', 'name': 'message.delta', 'clock': 100.5})
add('run_event', **{'run_id': 'r1', 'name': 'message.delta', 'fields': {'delta': 'hi'}, 'clock': 100.5})
add('run_event', **{'run_id': 'r1', 'name': 'tool.started', 'fields': {'event': 'override', 'run_id': 'other', 'timestamp': 9, 'extra': 1}, 'clock': 100.5})
add('run_event', **{'run_id': 'r1', 'name': 'run.completed', 'fields': {}, 'clock': [1, 2]})
add('run_event', **{'run_id': 'r1', 'name': 'run.completed', 'fields': {'seq': 0, 'flag': False, 'nothing': None}, 'clock': 7})
add('room_permission_for', **{'request': {'path': '/v1/runs/r1/stop', 'method': 'POST'}})
add('room_permission_for', **{'request': {'path': '/v1/runs/r1/resolve-unknown', 'method': 'POST'}})
add('room_permission_for', **{'request': {'path': '/v1/runs/r1/approval', 'method': 'POST'}})
add('room_permission_for', **{'request': {'path': '/v1/runs/r1/clarify', 'method': 'POST'}})
add('room_permission_for', **{'request': {'path': '/v1/runs', 'method': 'GET'}})
add('room_permission_for', **{'request': {'path': '/v1/runs', 'method': 'POST'}})
add('room_permission_for', **{'request': {'path': '/v1/runs/r1/events', 'method': 'GET'}})
add('room_permission_for', **{'request': {'path': '/stopx', 'method': 'POST'}})
add('room_permission_for', **{'request': {'path': '/v1/runs/r1/stop', 'method': 'GET'}})
add('room_run_scope', **{'claims': {'room_id': 'room1', 'home_install_id': 'home1', 'authority_gateway_id': 'gw1', 'authority_epoch': 3, 'member_id': 'mem1', 'target_install_id': 'inst1', 'target_profile': 'default'}})
add('room_run_scope', **{'claims': {'room_id': 'room1', 'home_install_id': 'home1', 'authority_gateway_id': 'gw1', 'authority_epoch': 3, 'target_install_id': 'inst1', 'target_profile': 'default'}})
add('room_run_scope', **{'claims': {'room_id': 'room1', 'home_install_id': 'home1', 'authority_gateway_id': 'gw1', 'authority_epoch': 3.5, 'member_id': 'mem1', 'target_install_id': 'inst1', 'target_profile': 'default'}})
add('room_run_scope', **{'claims': {'room_id': 'room1', 'home_install_id': 'home1', 'authority_gateway_id': 'gw1', 'authority_epoch': 3, 'member_id': 'mem1', 'target_install_id': 'inst1', 'target_profile': None}})
add('room_run_scope', **{'claims': {'room_id': 'room1', 'home_install_id': 'home1', 'authority_gateway_id': 'gw1', 'authority_epoch': 3, 'member_id': True, 'target_install_id': 'inst1', 'target_profile': 'default'}})
add('room_run_scope', **{'claims': {'room_id': '', 'home_install_id': '', 'authority_gateway_id': '', 'authority_epoch': '', 'member_id': '', 'target_install_id': '', 'target_profile': ''}})
add('remember_room_retention', **{'claims': {'status_expires_at': 12.5}})
add('remember_room_retention', **{'claims': {'expires_at': '99'}})
add('remember_room_retention', **{'claims': {}})
add('remember_room_retention', **{'claims': {'status_expires_at': None, 'expires_at': 7}})
add('remember_room_retention', **{'claims': {'status_expires_at': 4.25}, 'request_set_raises': 'AttributeError'})
add('remember_room_retention', **{'claims': {'expires_at': 8}, 'request_set_raises': 'TypeError'})
add('remember_room_retention', **{'claims': {'status_expires_at': 0, 'expires_at': 5}})
add('run_idempotency_scope', **{'room_token': 'tok', 'request': {'path': '/v1/runs/r1/stop', 'method': 'POST'}, 'claims_ret': {'room_id': 'room1', 'home_install_id': 'home1', 'authority_gateway_id': 'gw1', 'authority_epoch': 3, 'member_id': 'mem1', 'target_install_id': 'inst1', 'target_profile': 'default'}})
add('run_idempotency_scope', **{'profile': 'p1', 'api_key_ret': 'k1'})
add('run_idempotency_scope', **{'profile': None, 'api_key_ret': None})
add('run_idempotency_scope', **{'profile': '', 'api_key_ret': ''})
add('run_idempotency_scope', **{'room_token': '', 'profile': 'p2', 'api_key_ret': 'k2'})
add('run_idempotency_scope', **{'room_token': 'tok', 'request': {'path': '/v1/runs', 'method': 'POST'}, 'claims_raises': True, 'claims_exc_type': 'ValueError', 'claims_exc_msg': 'bad grant'})
add('check_run_auth', **{'permission': 'status'})
add('check_run_auth', **{'permission': 'status', 'check_auth_ret': {'status': 401, 'body': {'error': 'nope'}, 'headers': None}})
add('check_run_auth', **{'room_token': 'tok', 'permission': 'approve', 'claims_ret': {'room_id': 'room1', 'home_install_id': 'home1', 'authority_gateway_id': 'gw1', 'authority_epoch': 3, 'member_id': 'mem1', 'target_install_id': 'inst1', 'target_profile': 'default'}, 'request': {'path': '/v1/runs/r1/approval', 'method': 'POST'}})
add('check_run_auth', **{'room_token': 'tok', 'permission': 'stop', 'claims_raises': True, 'claims_exc_type': 'RuntimeError', 'claims_exc_msg': 'grant invalid'})
add('check_run_auth', **{'room_token': 'tok', 'permission': 'stop', 'claims_raises': True, 'claims_exc_type': 'RoomGrantReauthorizationRequired', 'claims_exc_msg': 'room grant is revoked'})
add('owner_alive', **{'owner_pid': 0, 'owner_started': 0})
add('owner_alive', **{'owner_pid': 5, 'owner_started': 0, 'pid_exists_ret': False})
add('owner_alive', **{'owner_pid': 5, 'owner_started': 0, 'pid_exists_ret': True})
add('owner_alive', **{'owner_pid': 5, 'owner_started': 100, 'pid_exists_ret': True, 'start_time_ret': 100, 'fp_match_ret': True})
add('owner_alive', **{'owner_pid': 5, 'owner_started': 100, 'pid_exists_ret': True, 'start_time_ret': 200, 'fp_match_ret': False})
add('owner_alive', **{'owner_pid': 5, 'owner_started': 0, 'pid_exists_raises': True})
add('owner_alive', **{'owner_pid': 5, 'owner_started': 100, 'pid_exists_ret': True, 'start_time_ret': None, 'fp_match_ret': False})
add('owner_alive', **{'owner_pid': 5, 'owner_started': 100, 'pid_exists_ret': True, 'start_time_ret': 100, 'fp_match_raises': True})
add('set_run_status', **{'run_id': 'r1', 'status': 'queued', 'clock': 10, 'fields': {'created_at': 10, 'session_id': 's', 'model': 'm'}})
add('set_run_status', **{'run_id': 'r1', 'status': 'completed', 'clock': 20, 'state': {'run_statuses': {'r1': {'status': 'running', 'created_at': 5}}, 'run_idempotency_ids': ['r1']}, 'fields': {'output': 'done'}})
add('set_run_status', **{'run_id': 'r1', 'status': 'running', 'clock': 20, 'state': {'run_statuses': {'r1': {'status': 'running', 'created_at': 5}}, 'run_idempotency_ids': ['r1']}})
add('set_run_status', **{'run_id': 'r1', 'status': 'running', 'clock': 20, 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_idempotency_ids': ['r1']}, 'fields': {'output': 'x'}})
add('set_run_status', **{'run_id': 'r1', 'status': 'waiting_for_approval', 'clock': 20, 'fields': {'approval': {'request_id': 'a1'}, 'last_event': 'approval.request'}})
add('set_run_status', **{'run_id': 'r1', 'status': 'running', 'clock': 20, 'state': {'run_statuses': {'r1': {'status': 'waiting_for_approval', 'approval': {'request_id': 'a1'}}}}})
add('set_run_status', **{'run_id': 'r1', 'status': 'running', 'clock': 20, 'state': {'run_shutdown_requested_at': 55.5}})
add('set_run_status', **{'run_id': 'r1', 'status': 'completed', 'clock': 20, 'state': {'run_shutdown_requested_at': 55.5}})
add('set_run_status', **{'run_id': 'r1', 'status': 'completed', 'clock': 20, 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_idempotency_ids': ['r1']}, 'store_update_raises': True, 'store_update_exc_type': 'OSError', 'store_update_exc_msg': 'disk full'})
add('set_run_status', **{'run_id': 'r1', 'status': 'queued', 'clock': 20, 'state': {'run_statuses': {'r1': {'status': 'queued', 'created_at': 5}}}, 'fields': {'created_at': 99}})
add('mark_shutdown_requested', **{'clock': 50})
add('mark_shutdown_requested', **{'clock': 50, 'state': {'run_statuses': {'r1': {'status': 'running'}, 'r2': {'status': 'completed'}, 'r3': {'status': None}}}})
add('mark_shutdown_requested', **{'clock': 50, 'state': {'run_statuses': {'r1': {'status': 'running', 'shutdown_requested_at': 10}}}})
add('mark_shutdown_requested', **{'clock': 50, 'state': {'run_shutdown_requested_at': 10, 'run_statuses': {'r1': {'status': 'running', 'shutdown_requested_at': 10}}}})
add('mark_shutdown_requested', **{'clock': 50, 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_idempotency_ids': ['r1']}})
add('forget_run', **{'run_id': 'r1', 'tables': ['run_streams', 'run_streams_created'], 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_streams': {'r1': {}}, 'run_streams_created': {'r1': 1}, 'run_owners': {'r1': 'scope1'}, 'run_approval_sessions': {'r1': 'r1'}, 'active_run_agents': {'r1': {}}, 'active_run_tasks': {'r1': {}}, 'run_idempotency_ids': ['r1'], 'stopping_run_ids': ['r1'], 'shutdown_interrupted_run_ids': ['r1']}})
add('forget_run', **{'run_id': 'r1', 'tables': ['run_statuses', 'active_run_agents', 'active_run_tasks', 'run_streams', 'run_approval_sessions', 'stopping_run_ids', 'shutdown_interrupted_run_ids', 'run_idempotency_ids'], 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_streams': {'r1': {}}, 'run_streams_created': {'r1': 1}, 'run_owners': {'r1': 'scope1'}, 'run_approval_sessions': {'r1': 'r1'}, 'active_run_agents': {'r1': {}}, 'active_run_tasks': {'r1': {}}, 'run_idempotency_ids': ['r1'], 'stopping_run_ids': ['r1'], 'shutdown_interrupted_run_ids': ['r1']}})
add('forget_run', **{'run_id': 'r1', 'tables': ['stopping_run_ids'], 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_streams': {'r1': {}}, 'run_streams_created': {'r1': 1}, 'run_owners': {'r1': 'scope1'}, 'run_approval_sessions': {'r1': 'r1'}, 'active_run_agents': {'r1': {}}, 'active_run_tasks': {'r1': {}}, 'run_idempotency_ids': ['r1'], 'stopping_run_ids': ['r1'], 'shutdown_interrupted_run_ids': ['r1']}})
add('forget_run', **{'run_id': 'rX', 'tables': ['run_statuses', 'run_streams'], 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_streams': {'r1': {}}, 'run_streams_created': {'r1': 1}, 'run_owners': {'r1': 'scope1'}, 'run_approval_sessions': {'r1': 'r1'}, 'active_run_agents': {'r1': {}}, 'active_run_tasks': {'r1': {}}, 'run_idempotency_ids': ['r1'], 'stopping_run_ids': ['r1'], 'shutdown_interrupted_run_ids': ['r1']}})
add('forget_run', **{'run_id': 'r1', 'tables': ['run_idempotency_ids'], 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_streams': {'r1': {}}, 'run_streams_created': {'r1': 1}, 'run_owners': {'r1': 'scope1'}, 'run_approval_sessions': {'r1': 'r1'}, 'active_run_agents': {'r1': {}}, 'active_run_tasks': {'r1': {}}, 'run_idempotency_ids': ['r1'], 'stopping_run_ids': ['r1'], 'shutdown_interrupted_run_ids': ['r1']}})
add('forget_run', **{'run_id': 'r1', 'tables': ['run_statuses', 'active_run_agents', 'active_run_tasks', 'run_streams', 'run_approval_sessions'], 'state': {'run_statuses': {'r1': {}}, 'active_run_agents': {'r1': {}}, 'active_run_tasks': {'r1': {}}, 'run_streams': {'r1': {}}, 'run_approval_sessions': {'r1': 'r1'}}})
add('release_run_owner_if_forgotten', **{'run_id': 'r1', 'state': {'run_statuses': {'r1': {}}, 'run_owners': {'r1': 'scope1'}}})
add('release_run_owner_if_forgotten', **{'run_id': 'r1', 'state': {'active_run_agents': {'r1': {}}, 'run_owners': {'r1': 'scope1'}}})
add('release_run_owner_if_forgotten', **{'run_id': 'r1', 'state': {'active_run_tasks': {'r1': {}}, 'run_owners': {'r1': 'scope1'}}})
add('release_run_owner_if_forgotten', **{'run_id': 'r1', 'state': {'run_streams': {'r1': {}}, 'run_owners': {'r1': 'scope1'}}})
add('release_run_owner_if_forgotten', **{'run_id': 'r1', 'state': {'run_approval_sessions': {'r1': {}}, 'run_owners': {'r1': 'scope1'}}})
add('release_run_owner_if_forgotten', **{'run_id': 'r1', 'state': {'run_owners': {'r1': 'scope1'}}})
add('release_run_owner_if_forgotten', **{'run_id': 'r1', 'state': {}})
add('durable_run_status', **{'run_id': 'r1', 'projection_ret': {'status': 'completed', 'output': 'o'}, 'state': {'run_statuses': {'r1': {'model': 'm', 'runtime': {'provider': 'p'}, 'created_at': 1, 'extra': 'x'}}}})
add('durable_run_status', **{'run_id': 'r1', 'state': {'run_statuses': {'r1': {'status': 'running', 'object': 'hermes.run'}}}})
add('durable_run_status', **{'run_id': 'r1', 'profile': 'p1', 'api_key_ret': 'k1', 'retention_until': 12.5, 'state': {'run_statuses': {'r1': {'status': 'running'}}, 'run_idempotency_ids': ['r1']}})
add('durable_run_status', **{'run_id': 'r1', 'profile': 'p1', 'api_key_ret': 'k1'})
add('durable_run_status', **{'run_id': 'r1', 'profile': 'p1', 'api_key_ret': 'k1', 'store_record': {'status': {'status': 'completed', 'output': 'done'}, 'owner_pid': 0}})
add('durable_run_status', **{'run_id': 'r1', 'profile': 'p1', 'api_key_ret': 'k1', 'store_record': {'status': {'status': 'running'}, 'owner_pid': 9, 'owner_started': 0}, 'pid_exists_ret': True})
add('durable_run_status', **{'run_id': 'r1', 'profile': 'p1', 'api_key_ret': 'k1', 'clock': 77, 'store_record': {'status': {'status': 'running'}, 'owner_pid': 9, 'owner_started': 0}, 'pid_exists_ret': False})
add('durable_run_status', **{'run_id': 'r1', 'profile': 'p1', 'api_key_ret': 'k1', 'store_record': {'status': {'status': 'queued'}, 'owner_pid': '7', 'owner_started': None}, 'pid_exists_ret': True})
add('durable_run_status', **{'run_id': 'r1', 'room_token': 'tok', 'request': {'path': '/v1/runs/r1', 'method': 'GET'}, 'claims_ret': {'room_id': 'room1', 'home_install_id': 'home1', 'authority_gateway_id': 'gw1', 'authority_epoch': 3, 'member_id': 'mem1', 'target_install_id': 'inst1', 'target_profile': 'default'}, 'retention_until': 12.5, 'store_record': {'status': {'status': 'failed', 'error': 'e'}, 'owner_pid': 0}})
add('resolve_conversation_history', **{'body': {'conversation_history': [{'role': 'user', 'content': 'a'}, {'role': 'assistant', 'content': 'b'}], 'instructions': 'i'}})
add('resolve_conversation_history', **{'body': {'conversation_history': 'nope', 'instructions': 'i'}})
add('resolve_conversation_history', **{'body': {'conversation_history': [{'role': 'user'}]}})
add('resolve_conversation_history', **{'body': {'conversation_history': [{'role': 'u', 'content': 'c'}, 5]}})
add('resolve_conversation_history', **{'body': {'conversation_history': [{'role': 'user', 'content': 'a'}], 'previous_response_id': 'p1'}})
add('resolve_conversation_history', **{'body': {'previous_response_id': 'p1'}, 'stored_ret': {'conversation_history': [{'role': 'user', 'content': 'h'}], 'session_id': 's1', 'instructions': 'si'}})
add('resolve_conversation_history', **{'body': {'previous_response_id': 'p1', 'instructions': 'keep'}, 'stored_ret': {'conversation_history': [{'role': 'user', 'content': 'h'}], 'session_id': 's1', 'instructions': 'si'}})
add('resolve_conversation_history', **{'body': {'previous_response_id': 'p1'}, 'stored_ret': None})
add('resolve_conversation_history', **{'body': {}, 'raw_input': [{'role': 'user', 'content': 'a'}, {'role': 'assistant', 'content': [{'type': 'text', 'text': 'b'}, {'type': 'image', 'url': 'u'}, {'type': 'text', 'text': 'c'}]}, {'role': 'user', 'content': 'last'}]})
add('resolve_conversation_history', **{'body': {}, 'raw_input': [{'role': 'user', 'content': 'only'}]})
add('resolve_conversation_history', **{'body': {}, 'raw_input': [{'role': '', 'content': 'x'}, {'content': 'y'}, {'role': 'user', 'content': ''}, {'role': 'user', 'content': 'z'}, {'role': 'user', 'content': 'last'}]})
add('resolve_conversation_history', **{'body': {'conversation_history': [{'role': 5, 'content': True}]}})
add('accepted_response', **{'run_id': 'r1', 'status': 'started', 'replayed': False})
add('accepted_response', **{'run_id': 'r1', 'status': 'started', 'replayed': True})
add('accepted_response', **{'run_id': 'r1', 'status': 'started', 'gateway_session_key': 'k', 'replayed': False})
add('accepted_response', **{'run_id': 'r1', 'status': 'started', 'gateway_session_key': 'k', 'replayed': True})
add('accepted_response', **{'run_id': 'r1', 'status': 'started', 'gateway_session_key': '', 'replayed': True})
add('replay_or_conflict', **{'outcome': 'conflict', 'record': {'run_id': 'r1', 'status': {'status': 'queued'}}})
add('replay_or_conflict', **{'outcome': 'reused', 'record': {'run_id': 'r1', 'status': {'status': 'queued'}}, 'state': {'run_statuses': {'r1': {'status': 'running', 'object': 'hermes.run', 'run_id': 'r1'}}}})
add('replay_or_conflict', **{'outcome': 'reused', 'record': {'run_id': 'r1', 'status': {'status': 'queued'}}, 'profile': 'p1', 'api_key_ret': 'k1'})
add('replay_or_conflict', **{'outcome': 'reused', 'record': {'run_id': 'r1', 'status': {}}, 'profile': 'p1', 'api_key_ret': 'k1'})
add('replay_or_conflict', **{'outcome': 'reused', 'record': {'run_id': 'r1', 'status': {'status': 'completed'}}, 'state': {'run_statuses': {'r1': {}}}})
add('replay_or_conflict', **{'outcome': 'reused', 'record': {'run_id': 42, 'status': {'status': 'queued'}}, 'profile': 'p1', 'api_key_ret': 'k1'})
add('resolve_conversation_history', **{'body': {'previous_response_id': 'p1', 'instructions': None}, 'stored_ret': {'conversation_history': [{'role': 'user', 'content': 'h'}], 'session_id': 's1', 'instructions': 'si'}})
add('remember_room_retention', **{'claims': {'expires_at': 3}, 'request_set_raises': 'RuntimeError'})

# ── _call_id (pure leaf) ──
add("call_id", kw={})
add("call_id", kw={"tool_call_id": "c1"})
add("call_id", kw={"tool_call_id": ""})
add("call_id", kw={"tool_call_id": None})
add("call_id", kw={"tool_call_id": 0})
add("call_id", kw={"tool_call_id": 123})
add("call_id", kw={"other": "x"})
add("call_id", kw={"tool_call_id": False})

# ── _run_user_message: non-dict last element -> .get AttributeError ──
add("run_user_message", raw_input=["s"])
add("run_user_message", raw_input=[True])
add("run_user_message", raw_input=[None])
add("run_user_message", raw_input=[[]])
add("run_user_message", raw_input=[5.5])
add("run_user_message", raw_input=[{"content": "a"}, 7])

# ── _run_event: positional/kwarg collision -> TypeError (no clock seam) ──
add("run_event", run_id="r1", name="n", fields={"name": "y"}, clock=1)
add("run_event", run_id="r1", name="n", fields={"name": "y", "run_id": "x"}, clock=1)
add("run_event", run_id="r1", name="n", fields={"run_id": "x", "name": "y"}, clock=1)

# ── _durable_run_status: present-null status falls through to hydration ──
add("durable_run_status", run_id="r1", profile="p1", api_key_ret="k1",
    state={"run_statuses": {"r1": None}},
    store_record={"status": {"status": "completed"}, "owner_pid": 0})

# ── _tool_completed_preview: json.dumps escaping + UTF-8 passthrough ──
add("tool_completed_preview", result={"msg": 'he said "hi"\nbye', "u": "caf\u00e9"})

# ── terminal_run_status: str() coercion of a numeric exit reason + steer copy ──
add("terminal_run_status", result={"failed": True, "turn_exit_reason": 42,
                                   "pending_steer": {"text": "go"}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            counts[c["func"]] = counts.get(c["func"], 0) + 1
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, n in counts.items() if n < 5]
    if thin:
        raise SystemExit(f"FATAL: funcs with <5 cases: {thin}")


if __name__ == "__main__":
    main()

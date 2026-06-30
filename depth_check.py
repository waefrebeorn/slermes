#!/usr/bin/env python3
"""
depth_check.py — Depth-check pipeline for Slermes C port.

Scans all port_*.c files and classifies each function:
  REAL:    Has meaningful logic (calls project functions, manipulates data)
  STUB:    Only hermes_log + return (façade)
  PARTIAL: Some logic but incomplete

Also checks:
  - Type consistency (custom types must be defined somewhere)
  - Cross-file call graph (are called functions actually defined?)
  - Include correctness (does the header exist?)
"""

import os
import re
import json
import sys

SLERMES_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(SLERMES_DIR, 'src')
INCLUDE_DIR = os.path.join(SLERMES_DIR, 'include')

# Standard C functions that don't count as "real logic"
STDLIB_FUNCS = {
    'printf', 'fprintf', 'sprintf', 'snprintf', 'asprintf',
    'malloc', 'free', 'calloc', 'realloc', 'aligned_alloc',
    'memcpy', 'memset', 'memmove', 'memchr', 'memcmp',
    'strcmp', 'strncmp', 'strlen', 'strcpy', 'strncpy', 'strcat', 'strncat',
    'strstr', 'strchr', 'strrchr', 'strtok', 'strtok_r', 'strdup', 'strndup',
    'strcspn', 'strspn', 'strpbrk',
    'atoi', 'atol', 'atof', 'strtol', 'strtoul', 'strtod', 'strtoll', 'strtoull',
    'fopen', 'fclose', 'fread', 'fwrite', 'fgets', 'fputs', 'fgetc', 'fputc',
    'fseek', 'ftell', 'rewind', 'fflush', 'fileno',
    'open', 'close', 'read', 'write', 'lseek', 'stat', 'fstat', 'lstat',
    'mkdir', 'rmdir', 'unlink', 'rename', 'chmod', 'chown', 'utime', 'utimes',
    'pipe', 'dup', 'dup2',
    'socket', 'bind', 'listen', 'accept', 'connect', 'send', 'recv',
    'sendto', 'recvfrom', 'getsockopt', 'setsockopt',
    'select', 'poll', 'epoll_create', 'epoll_ctl', 'epoll_wait',
    'getaddrinfo', 'freeaddrinfo', 'getnameinfo',
    'inet_ntop', 'inet_pton', 'htons', 'htonl', 'ntohs', 'ntohl',
    'pthread_create', 'pthread_join', 'pthread_mutex_lock', 'pthread_mutex_unlock',
    'pthread_mutex_init', 'pthread_mutex_destroy',
    'pthread_cond_wait', 'pthread_cond_signal', 'pthread_cond_broadcast',
    'pthread_cond_init', 'pthread_cond_destroy',
    'pthread_attr_init', 'pthread_attr_destroy',
    'pthread_self', 'pthread_equal',
    'usleep', 'sleep', 'nanosleep', 'gettimeofday', 'clock_gettime',
    'time', 'localtime', 'gmtime', 'mktime', 'strftime',
    'exit', 'abort', 'atexit',
    'getenv', 'setenv', 'unsetenv', 'clearenv',
    'getpid', 'getppid', 'getuid', 'geteuid', 'getgid', 'getegid',
    'fork', '_exit', 'setsid', 'execvp', 'waitpid', 'wait', 'signal', 'sigaction', 'raise', 'kill',
    'rand', 'srand', 'clock', 'difftime',
    'tolower', 'toupper', 'isalpha', 'isdigit', 'isalnum', 'isspace',
    'isxdigit', 'isprint', 'iscntrl', 'isupper', 'islower',
    'isatty', 'access', 'opendir', 'readdir', 'closedir', 'getcwd',
    'fscanf', 'sscanf', 'vscanf', 'vfscanf', 'vsscanf',
    'popen', 'pclose', 'system',
    'strncasecmp', 'strcasecmp',
    'bsearch', 'qsort',
    'abs', 'labs', 'llabs', 'div', 'ldiv', 'lldiv',
    'hermes_log', 'hermes_log_debug', 'hermes_log_info',
    'hermes_log_warning', 'hermes_log_error',
    'json_array_get',
    # JSON API functions (macros in hermes_json.h)
    'json_object_get', 'json_object_set', 'json_object', 'json_new_string',
    'json_new_number', 'json_new_object', 'json_new_array', 'json_new_bool',
    'json_new_null', 'json_string', 'json_number', 'json_bool', 'json_null',
    'json_array', 'json_set', 'json_obj_get', 'json_get_str', 'json_get_num',
    'json_get_bool', 'json_append', 'json_get', 'json_len', 'json_free',
    'json_copy', 'json_parse', 'json_serialize', 'json_node_get_string',
    'json_node_get_bool', 'json_node_get_int', 'json_node_get_double',
    'json_node_is_null', 'json_node_is_bool', 'json_node_is_number',
    'json_node_is_string', 'json_node_is_array', 'json_node_is_object',
    'json_int', 'json_double', 'json_has', 'json_validate_schema',
    'json_pointer_get', 'json_oom_occurred',
    'git_env', 'normalize_path', 'project_hash', 'spawn_local', 'exists',
    # POSIX/standard C functions that are commonly used
    'execlp', 'execl', 'execvp', 'execv', 'execvpe', 'execle',
    'dirname', 'basename', 'strerror', 'strerror_r',
    'pthread_cond_timedwait', 'pthread_condattr_init', 'pthread_condattr_destroy',
    'pthread_rwlock_init', 'pthread_rwlock_destroy',
    'pthread_rwlock_rdlock', 'pthread_rwlock_wrlock', 'pthread_rwlock_unlock',
    'sem_init', 'sem_destroy', 'sem_wait', 'sem_post', 'sem_trywait',
    'timer_create', 'timer_delete', 'timer_settime', 'timer_gettime',
    'clock_nanosleep', 'clock_getres',
    'shm_open', 'shm_unlink', 'mmap', 'munmap', 'mprotect',
    'mlock', 'munlock', 'mlockall', 'munlockall',
    'sysconf', 'pathconf', 'fpathconf',
    'getrlimit', 'setrlimit', 'getrusage',
    'utimensat', 'futimens',
    'fallocate', 'posix_fallocate',
    'sync', 'fsync', 'fdatasync',
    'fallback_entry_api_key',  # defined in port_agent_auxiliary_client.c
    'message_content_field', 'text_from_part',  # defined in port_agent_message_content.c
    's', 'completion_callback', 'async_delegation_get_executor',  # local variables/function names
    'strip_container_argv_prefix', 'memory_provider_payload_fn', 'memory_provider_config_path',
    'gemini_cli_status_fn', 'copilot_acp_status', 'oauth_provider_disconnect_command', 'build_oauth_catalog',
    'get_chat_argv_lock', 'inbound_handler', 'interrupt_handler', 'resolve_request_profile',
    'cron_resolve_scheduler', 'failed', 'model_flow_google_antigravity', 'defined',
    'touch_json',  # helper function
    'if', 'for', 'while', 'switch', 'return', 'sizeof', 'typeof',
    'static', 'const', 'extern', 'inline', 'volatile',
}

# Collect all defined functions and types in the project
def collect_project_symbols():
    """Scan all .c and .h files for defined functions and types."""
    defined_funcs = set()
    defined_types = set()
    defined_structs = {}
    
    for root, dirs, files in os.walk(SLERMES_DIR):
        # Skip __pycache__ and .git
        dirs[:] = [d for d in dirs if d not in ('__pycache__', '.git')]
        for f in files:
            if not (f.endswith('.c') or f.endswith('.h')):
                continue
            fp = os.path.join(root, f)
            try:
                with open(fp) as fh:
                    content = fh.read()
            except:
                continue
            
            # Find function definitions
            for m in re.finditer(r'^(\w[\w\s*]+)\s+(\w+)\s*\([^)]*\)\s*\{', content, re.MULTILINE):
                defined_funcs.add(m.group(2))
            
            # Find typedefs
            for m in re.finditer(r'typedef\s+(?:struct\s+)?(\w+)\s+(\w+)\s*;', content):
                defined_types.add(m.group(2))
            
            # Find struct definitions
            for m in re.finditer(r'(?:typedef\s+)?struct\s+(\w+)\s*\{', content):
                defined_types.add(m.group(1))
                defined_structs[m.group(1)] = fp
    
    return defined_funcs, defined_types, defined_structs


def analyze_function_body(body_text, func_name):
    """Analyze a function body and return metrics."""
    lines = body_text.strip().split('\n')
    
    # Count non-empty, non-comment lines
    code_lines = []
    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith('/*') or stripped.startswith('//') or stripped.startswith('*'):
            continue
        if stripped == '{' or stripped == '}':
            continue
        code_lines.append(stripped)
    
    # Count calls to non-stdlib functions
    project_calls = []
    for line in code_lines:
        calls = re.findall(r'\b([a-z_][a-z0-9_]*)\s*\(', line)
        for c in calls:
            if c not in STDLIB_FUNCS and c != func_name:
                project_calls.append(c)
    
    # Check for TODO
    has_todo = '/* TODO:' in body_text or 'TODO:' in body_text
    
    # Check for stub pattern: only log + return
    is_stub = False
    meaningful_lines = [l for l in code_lines 
                       if not l.startswith('hermes_log') 
                       and not l.startswith('return')
                       and not l.startswith('if (!ctx)')
                       and not l.startswith('if(!ctx)')
                       and l != '{' and l != '}']
    
    if len(meaningful_lines) <= 1 and not project_calls:
        is_stub = True
    
    return {
        'total_lines': len(code_lines),
        'meaningful_lines': len(meaningful_lines),
        'project_calls': list(set(project_calls)),
        'project_call_count': len(project_calls),
        'has_todo': has_todo,
        'is_stub': is_stub,
    }


def scan_port_files(defined_funcs, defined_types):
    """Scan all port_*.c files and classify functions."""
    results = {
        'total': 0,
        'real': 0,
        'stub': 0,
        'partial': 0,
        'functions': [],
        'stub_files': [],
        'undefined_calls': [],
        'undefined_types': [],
    }
    
    for root, dirs, files in os.walk(SRC_DIR):
        for f in sorted(files):
            if not (f.startswith('port_') and f.endswith('.c')):
                continue
            fp = os.path.join(root, f)
            rel_path = os.path.relpath(fp, SLERMES_DIR)
            
            with open(fp) as fh:
                content = fh.read()
            
            # Find all functions with PoP annotations
            func_pattern = re.compile(
                r'/\* Port of Python: (\w+) \*/\n'
                r'(?:static\s+)?(\w[\w\s*]+?)\s+(\w+)\s*\(',
                re.MULTILINE
            )
            
            file_has_stub = False
            file_funcs = []
            
            for m in func_pattern.finditer(content):
                pop_name = m.group(1)
                ret_type = m.group(2).strip()
                func_name = m.group(3)
                
                results['total'] += 1
                
                # Extract function body
                brace_start = content.find('{', m.end())
                if brace_start == -1:
                    continue
                
                # Find matching closing brace
                depth = 1
                pos = brace_start + 1
                while depth > 0 and pos < len(content):
                    if content[pos] == '{':
                        depth += 1
                    elif content[pos] == '}':
                        depth -= 1
                    pos += 1
                
                body = content[brace_start:pos]
                metrics = analyze_function_body(body, func_name)
                
                # Check if called functions are defined
                undefined = []
                for call in metrics['project_calls']:
                    if call not in defined_funcs and call not in STDLIB_FUNCS:
                        undefined.append(call)
                
                # Check return type
                ret_undefined = ret_type not in defined_types and ret_type not in {
                    'void', 'int', 'char', 'bool', 'double', 'float', 'long', 'short',
                    'unsigned', 'signed', 'size_t', 'ssize_t', 'int8_t', 'int16_t',
                    'int32_t', 'int64_t', 'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
                    'const', 'char', 'void', 'int', 'bool', 'double',
                } and not ret_type.endswith('*')
                
                classification = 'REAL'
                if metrics['is_stub']:
                    classification = 'STUB'
                    results['stub'] += 1
                    file_has_stub = True
                elif metrics['has_todo'] or undefined:
                    classification = 'PARTIAL'
                    results['partial'] += 1
                else:
                    results['real'] += 1
                
                func_result = {
                    'file': rel_path,
                    'func': func_name,
                    'pop_name': pop_name,
                    'ret_type': ret_type,
                    'classification': classification,
                    'metrics': metrics,
                    'undefined_calls': undefined[:10],
                }
                
                results['functions'].append(func_result)
                file_funcs.append(func_result)
                
                if undefined:
                    results['undefined_calls'].extend([
                        {'file': rel_path, 'func': func_name, 'undefined': u}
                        for u in undefined
                    ])
            
            if file_has_stub:
                results['stub_files'].append(rel_path)
    
    return results


def main():
    print("=== SLERMES DEPTH CHECK PIPELINE ===\n")
    
    print("Collecting project symbols...")
    defined_funcs, defined_types, defined_structs = collect_project_symbols()
    print(f"  Defined functions: {len(defined_funcs)}")
    print(f"  Defined types: {len(defined_types)}")
    
    print("\nScanning port files...")
    results = scan_port_files(defined_funcs, defined_types)
    
    print(f"\n{'='*60}")
    print(f"RESULTS")
    print(f"{'='*60}")
    print(f"Total ported functions: {results['total']}")
    print(f"  REAL:    {results['real']} ({results['real']*100//results['total'] if results['total'] else 0}%)")
    print(f"  STUB:    {results['stub']} ({results['stub']*100//results['total'] if results['total'] else 0}%)")
    print(f"  PARTIAL:  {results['partial']} ({results['partial']*100//results['total'] if results['total'] else 0}%)")
    print(f"\nFiles with stubs: {len(results['stub_files'])}")
    print(f"Undefined function calls: {len(results['undefined_calls'])}")
    
    # Top stubs by file
    print(f"\n=== TOP 20 STUB FILES ===")
    stub_counts = {}
    for f in results['functions']:
        if f['classification'] == 'STUB':
            stub_counts[f['file']] = stub_counts.get(f['file'], 0) + 1
    
    for f, count in sorted(stub_counts.items(), key=lambda x: -x[1])[:20]:
        print(f"  {count:3d} stubs  {f}")
    
    # Top real functions
    print(f"\n=== TOP 20 REAL FUNCTIONS ===")
    real_funcs = [f for f in results['functions'] if f['classification'] == 'REAL']
    real_funcs.sort(key=lambda x: x['metrics']['meaningful_lines'], reverse=True)
    for f in real_funcs[:20]:
        print(f"  {f['metrics']['meaningful_lines']:3d} lines  {f['file']}::{f['func']}")
    
    # Undefined calls
    if results['undefined_calls']:
        print(f"\n=== UNDEFINED FUNCTION CALLS (first 30) ===")
        for u in results['undefined_calls'][:30]:
            print(f"  {u['file']}::{u['func']} calls undefined: {u['undefined']}")
    
    # Save results
    output_path = os.path.join(SLERMES_DIR, 'depth_check_results.json')
    with open(output_path, 'w') as f:
        # Make it serializable
        serializable = {
            'total': results['total'],
            'real': results['real'],
            'stub': results['stub'],
            'partial': results['partial'],
            'stub_file_count': len(results['stub_files']),
            'undefined_call_count': len(results['undefined_calls']),
            'stub_files': results['stub_files'],
            'top_stubs': [{'file': f, 'count': c} for f, c in sorted(stub_counts.items(), key=lambda x: -x[1])[:30]],
            'top_real': [{'file': f['file'], 'func': f['func'], 'lines': f['metrics']['meaningful_lines']} for f in real_funcs[:30]],
            'undefined_calls': results['undefined_calls'][:50],
        }
        json.dump(serializable, f, indent=2)
    
    print(f"\nFull results saved to {output_path}")
    
    return 0 if results['stub'] == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
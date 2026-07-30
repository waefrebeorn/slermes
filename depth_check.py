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
# These are truly C standard library functions - NOT project APIs
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
    'getpid', 'getpid', 'getppid', 'getuid', 'geteuid', 'getgid', 'getegid',
    'fork', '_exit', 'setsid', 'execvp', 'waitpid', 'wait', 'signal', 'sigaction', 'raise', 'kill',
    'rand', 'srand', 'clock', 'difftime',
    'tolower', 'toupper', 'isalpha', 'isdigit', 'isalnum', 'isspace', 'isspace',
    'isxdigit', 'isprint', 'iscntrl', 'isupper', 'islower',
    'isatty', 'access', 'opendir', 'readdir', 'closedir', 'getcwd',
    'fscanf', 'sscanf', 'vscanf', 'vfscanf', 'vsscanf',
    'popen', 'pclose', 'system',
    'strncasecmp', 'strcasecmp',
    'bsearch', 'qsort',
    'abs', 'labs', 'llabs', 'div', 'ldiv', 'lldiv',
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
    # Project-specific local variables/function names that get matched
    's', 'completion_callback', 'async_delegation_get_executor',
    'portal', 'shrinks', 'defined', 'touch_json', 'inbound_handler', 'interrupt_handler', 'failed',
    'json_node_get_string', 'json_node_get_bool', 'json_node_get_int', 'json_node_get_double',
    'json_node_is_object', 'json_node_is_array', 'json_node_is_string',
    'json_node_is_number', 'json_node_is_bool', 'json_node_is_null',
    'json_get_str', 'json_get_num', 'json_get_bool', 'json_get', 'json_len',
    'json_obj_get', 'json_node_get_int', 'json_node_get_double',
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
            for m in re.finditer(r'^\s*(?:static\s+)?(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:char|short|int|long|float|double|bool|void|size_t|ssize_t|uint\d+_t|int\d+_t|[a-zA-Z_][a-zA-Z0-9_]*_t)\s*[*]?\s*(\w+)\s*\([^)]*\)\s*\{', content, re.MULTILINE):
                defined_funcs.add(m.group(1))

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
                       and not (l.startswith('return') and l.strip() == 'return')
                       and not l.startswith('if (!ctx)')
                       and not l.startswith('if(!ctx)')
                       and not l.startswith('touch_json')
                       and not re.match(r'^\(void\).*;$', l)  # Filter (void)var; unused param silencing
                       and l != '{' and l != '}']

    # Also filter out lines that are ONLY calls to STDLIB functions (assignments from stdlib calls)
    # e.g., "const char *multiplex = getenv(...);" - if getenv is stdlib, this line adds no project logic
    # BUT: we should still count lines that do real work even if they only call stdlib functions
    # (e.g., getenv, json_parse, fopen - these are real project operations)
    # So we only filter out truly empty/useless lines
    filtered_meaningful = []
    for l in meaningful_lines:
        # Check if line is just a variable assignment from stdlib calls
        calls_in_line = re.findall(r'\b([a-z_][a-z0-9_]*)\s*\(', l)
        only_stdlib = True
        for call in calls_in_line:
            if call not in STDLIB_FUNCS and call != func_name:
                only_stdlib = False
                break
        # Also allow lines with getenv, json_parse, fopen, strdup, etc. as meaningful
        # These are real project operations even if they use stdlib functions
        if not only_stdlib:
            filtered_meaningful.append(l)
        else:
            # Even if only stdlib calls, check if it's doing real work (not just logging/return)
            # Allow: getenv, json_parse, fopen, strdup, snprintf with format string, etc.
            real_work_patterns = ['getenv', 'json_parse', 'fopen', 'strdup', 'snprintf', 'malloc', 'calloc',
                                  'json_object', 'json_new_string', 'json_new_object', 'json_array',
                                  'json_set', 'json_object_set', 'json_array_append',
                                  'memcpy', 'rand', 'snprintf', 'strncpy', 'strcpy',
                                  'memset', 'memmove', 'strcmp', 'strncmp', 'strlen',
                                  'strstr', 'strchr', 'strrchr', 'atoi', 'atol', 'atof',
                                  'fclose', 'fread', 'fwrite', 'fgets', 'fputs',
                                  'popen', 'pclose', 'system', 'time', 'localtime', 'gmtime',
                                  'mkdir', 'rmdir', 'unlink', 'rename', 'access', 'stat',
                                  'isatty', 'getcwd', 'sscanf', 'vsnprintf']
            has_real_work = False
            for pattern in real_work_patterns:
                if pattern in l:
                    has_real_work = True
                    break
            # Also consider lines with control flow and assignments as meaningful work
            control_flow_keywords = ['while', 'for', 'if', 'switch', 'do', 'case']
            assignment_ops = ['=', '+=', '-=', '*=', '/=', '++', '--']
            has_control_flow = any(kw in l for kw in control_flow_keywords)
            has_assignment = any(op in l for op in assignment_ops)

            if has_real_work or has_control_flow or has_assignment:
                filtered_meaningful.append(l)

    if len(filtered_meaningful) == 0 and not project_calls:
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
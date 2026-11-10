/*
 * port_relaunch.c
 *
 * Pure helpers ported from hermes_cli/relaunch.py - unified self-relaunch.
 *
 * The module's real work is process replacement (os.execv) and reading
 * sys.executable / shutil.which. What is portable is argv construction:
 *
 *   rl_extract_inherited_flags <- relaunch.py:_extract_inherited_flags
 *   rl_build_relaunch_argv    <- relaunch.py:build_relaunch_argv
 *
 * The inherited-flag TABLE is normally derived by introspecting the argparse
 * parser (_build_inherited_flag_table); that is not expressible here. This port
 * uses the static PRE_ARGPARSE_INHERITED_FLAGS half, which is all the C side
 * can know without the parser.
 */

#include "port_relaunch.h"

#include <stdlib.h>
#include <string.h>

/* relaunch.py -> hermes_cli/_parser.py:PRE_ARGPARSE_INHERITED_FLAGS */
static const char *const RL_PRE_FLAGS[]  = { "--profile", "-p" };
static const int          RL_PRE_VALUES[] = { 1,           1  };
#define RL_PRE_N 2

/* ===========================================================================
 *  _extract_inherited_flags  (relaunch.py:_extract_inherited_flags)
 *
 *      flags = []
 *      i = 0
 *      while i < len(argv):
 *          arg = argv[i]
 *          if "=" in arg:
 *              if any(arg.split("=", 1)[0] == flag for flag, _ in TABLE):
 *                  flags.append(arg)
 *              i += 1
 *              continue
 *          for flag, takes_value in TABLE:
 *              if arg == flag:
 *                  flags.append(arg)
 *                  if takes_value and i + 1 < len(argv) and not argv[i+1].startswith("-"):
 *                      flags.append(argv[i + 1])
 *                      i += 1
 *                  break
 *          i += 1
 *      return flags
 *
 *  `table_flags` / `table_values` supply the extra (parser-derived) entries;
 *  pass NULL/0 for the static pre-argparse set only. Returns a newly allocated
 *  NULL-terminated array of borrowed pointers; the caller frees the ARRAY only.
 * =========================================================================== */

/* PoP: _extract_inherited_flags @ hermes_cli/relaunch.py:_extract_inherited_flags */
const char **rl_extract_inherited_flags(const char *const *argv,
                                        const char *const *table_flags,
                                        const int *table_values, int table_n,
                                        int *out_count)
{
    int n = 0;
    if (argv) while (argv[n]) n++;

    const char **out = (const char **)calloc((size_t)n * 2 + 2, sizeof(char *));
    if (!out) { if (out_count) *out_count = 0; return NULL; }

    int k = 0, i = 0;
    while (i < n) {
        const char *arg = argv[i];
        const char *eq = strchr(arg, '=');
        if (eq) {
            size_t klen = (size_t)(eq - arg);
            int hit = 0;
            for (int j = 0; j < RL_PRE_N; j++)
                if (strlen(RL_PRE_FLAGS[j]) == klen && strncmp(arg, RL_PRE_FLAGS[j], klen) == 0) { hit = 1; break; }
            if (!hit) for (int j = 0; j < table_n; j++)
                if (table_flags[j] && strlen(table_flags[j]) == klen &&
                    strncmp(arg, table_flags[j], klen) == 0) { hit = 1; break; }
            if (hit) out[k++] = arg;
            i += 1;
            continue;
        }
        int matched = 0;
        for (int j = 0; j < RL_PRE_N && !matched; j++) {
            if (strcmp(arg, RL_PRE_FLAGS[j]) != 0) continue;
            out[k++] = arg;
            if (RL_PRE_VALUES[j] && i + 1 < n && argv[i + 1][0] != '-') { out[k++] = argv[i + 1]; i += 1; }
            matched = 1;
        }
        for (int j = 0; j < table_n && !matched; j++) {
            if (!table_flags[j] || strcmp(arg, table_flags[j]) != 0) continue;
            out[k++] = arg;
            if (table_values[j] && i + 1 < n && argv[i + 1][0] != '-') { out[k++] = argv[i + 1]; i += 1; }
            matched = 1;
        }
        i += 1;
    }
    if (out_count) *out_count = k;
    return out;
}

/* ===========================================================================
 *  build_relaunch_argv  (relaunch.py:build_relaunch_argv)
 *
 *      bin_path = resolve_hermes_bin()
 *      argv = [bin_path] if bin_path else [sys.executable, "-m", "hermes_cli.main"]
 *      src = list(original_argv) if original_argv is not None else list(sys.argv[1:])
 *      if preserve_inherited:
 *          argv.extend(_extract_inherited_flags(src))
 *      argv.extend(extra_args)
 *      return argv
 *
 *  `bin_path` NULL selects the [python, "-m", "hermes_cli.main"] form; the
 *  caller supplies python_exe. `original_argv` NULL means "use sys.argv[1:]",
 *  which the caller supplies as `sys_argv`. Returns a newly allocated
 *  NULL-terminated array of borrowed pointers; free the ARRAY only.
 * =========================================================================== */

/* PoP: build_relaunch_argv @ hermes_cli/relaunch.py:build_relaunch_argv */
const char **rl_build_relaunch_argv(const char *bin_path, const char *python_exe,
                                    const char *const *original_argv,
                                    int preserve_inherited,
                                    const char *const *extra_args,
                                    int *out_count)
{
    int nsrc = 0, nextra = 0;
    const char *const *src = original_argv;
    if (src) while (src[nsrc]) nsrc++;
    if (extra_args) while (extra_args[nextra]) nextra++;

    const char **inh = NULL;
    int ninh = 0;
    if (preserve_inherited) {
        inh = rl_extract_inherited_flags(src, NULL, NULL, 0, &ninh);
    }

    const char **out = (const char **)calloc((size_t)nsrc + (size_t)nextra + (size_t)ninh + 8, sizeof(char *));
    if (!out) { free(inh); if (out_count) *out_count = 0; return NULL; }

    int k = 0;
    if (bin_path && bin_path[0]) {
        out[k++] = bin_path;
    } else {
        out[k++] = (python_exe && python_exe[0]) ? python_exe : "python";
        out[k++] = "-m";
        out[k++] = "hermes_cli.main";
    }
    for (int i = 0; i < ninh; i++) out[k++] = inh[i];
    for (int i = 0; i < nextra; i++) out[k++] = extra_args[i];
    if (out_count) *out_count = k;
    free(inh);
    return out;
}

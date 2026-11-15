/* t_port_pm_environments.c — oracle harness for pm/environments.py.
 * Emits one JSON line per case with a "fn" discriminator.
 *
 * project_root is passed in Python's own Windows canonical form (backslashes)
 * because install_key hashes it verbatim; path results are compared with the
 * separators normalised on the oracle side. */
#include "port_environments.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROJ "C:\\Users\\eman5\\hermes-agent"
#define ALT_PROJ "C:\\Users\\eman5\\slermes"
#define ROOT "C:\\Users\\eman5\\.hermes"
#define ALT_ROOT "C:\\Users\\eman5\\.hermes-alt"

static void jstr_escape(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar(ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", ch);
        } else {
            putchar(ch);
        }
    }
    putchar('"');
}

static void emit_str(int cs, const char *fn, const char *in, const char *out)
{
    printf("{\"case\":%d,\"fn\":\"%s\",\"in\":%s,\"out\":{\"str\":", cs, fn, in);
    jstr_escape(out ? out : "");
    printf("}}\n");
    fflush(stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    char b[8192];

    /* install_key — sha256(canonical)[:16] */
    emit_str(1, "install_key", "{\"project_root\":\"" PROJ "\"}",
             (pm_install_key(PROJ, b, sizeof(b)), b));
    emit_str(2, "install_key", "{\"project_root\":\"" ALT_PROJ "\"}",
             (pm_install_key(ALT_PROJ, b, sizeof(b)), b));

    /* installs_root */
    emit_str(3, "installs_root", "{\"root\":\"" ROOT "\"}",
             (pm_installs_root(ROOT, b, sizeof(b)), b));
    emit_str(4, "installs_root", "{\"root\":\"" ALT_ROOT "\"}",
             (pm_installs_root(ALT_ROOT, b, sizeof(b)), b));

    /* install_state_dir */
    emit_str(5, "install_state_dir",
             "{\"project_root\":\"" PROJ "\",\"root\":\"" ROOT "\"}",
             (pm_install_state_dir(PROJ, ROOT, b, sizeof(b)), b));
    emit_str(6, "install_state_dir",
             "{\"project_root\":\"" ALT_PROJ "\",\"root\":\"" ALT_ROOT "\"}",
             (pm_install_state_dir(ALT_PROJ, ALT_ROOT, b, sizeof(b)), b));

    /* runtime_facts_path */
    emit_str(7, "runtime_facts_path",
             "{\"project_root\":\"" PROJ "\",\"root\":\"" ROOT "\"}",
             (pm_runtime_facts_path(PROJ, ROOT, b, sizeof(b)), b));
    emit_str(8, "runtime_facts_path",
             "{\"project_root\":\"" ALT_PROJ "\",\"root\":\"" ALT_ROOT "\"}",
             (pm_runtime_facts_path(ALT_PROJ, ALT_ROOT, b, sizeof(b)), b));

    /* activation_inputs_dir */
    emit_str(9, "activation_inputs_dir",
             "{\"project_root\":\"" PROJ "\",\"root\":\"" ROOT "\"}",
             (pm_activation_inputs_dir(PROJ, ROOT, b, sizeof(b)), b));
    emit_str(10, "activation_inputs_dir",
             "{\"project_root\":\"" ALT_PROJ "\",\"root\":\"" ALT_ROOT "\"}",
             (pm_activation_inputs_dir(ALT_PROJ, ALT_ROOT, b, sizeof(b)), b));

    /* venv_bin_dir / venv_python, both layouts */
    {
        static const char *venvs[] = {"C:\\repo\\.venv", "C:\\repo\\venv",
                                      "/srv/venv", "/srv/.venv"};
        static const int wins[] = {1, 1, 0, 0};
        for (size_t i = 0; i < sizeof(venvs) / sizeof(venvs[0]); i++) {
            char in[256];
            snprintf(in, sizeof(in),
                     "{\"venv\":\"%s\",\"windows\":%s}", venvs[i],
                     wins[i] ? "true" : "false");
            pm_venv_bin_dir(venvs[i], wins[i], b, sizeof(b));
            emit_str((int)i + 11, "venv_bin_dir", in, b);
            pm_venv_python(venvs[i], wins[i], b, sizeof(b));
            emit_str((int)i + 15, "venv_python", in, b);
        }
        /* an explicit windows flag overriding the platform default */
        snprintf(b, sizeof(b), "%s", "x");
        pm_venv_bin_dir("/srv/venv", 1, b, sizeof(b));
        emit_str(19, "venv_bin_dir",
                 "{\"venv\":\"/srv/venv\",\"windows\":true}", b);
        pm_venv_python("/srv/venv", 0, b, sizeof(b));
        emit_str(20, "venv_python",
                 "{\"venv\":\"/srv/venv\",\"windows\":false}", b);
    }
    return 0;
}

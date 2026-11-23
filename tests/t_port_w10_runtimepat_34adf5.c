/* Port gate harness for hermes_cli/runtime_paths.py (gate w10_runtimepat_34adf5). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_runtimepat_34adf5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int jbool(const json_t *in,const char*k,int def){
    json_t*v=json_obj_get(in,k); return (v&&v->type==JSON_BOOL)?v->bool_val:def;
}
static int jint(const json_t *in,const char*k,int def){
    json_t*v=json_obj_get(in,k); return (v&&v->type==JSON_NUMBER)?(int)v->num_val:def;
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func,"install_key")) out=w10_runtimepat_34adf5_install_key(json_get_str(in,"canonical",""));
        else if(!strcmp(func,"venv_bin_dir")) out=w10_runtimepat_34adf5_venv_bin_dir(json_get_str(in,"venv",""),jbool(in,"windows",0));
        else if(!strcmp(func,"venv_python")) out=w10_runtimepat_34adf5_venv_python(json_get_str(in,"venv",""),jbool(in,"windows",0));
        else if(!strcmp(func,"site_packages")) out=w10_runtimepat_34adf5_site_packages(json_get_str(in,"venv",""),jbool(in,"windows",0),jint(in,"major",3),jint(in,"minor",11));
        else if(!strcmp(func,"install_state_dir")) out=w10_runtimepat_34adf5_install_state_dir(json_get_str(in,"home_root",""),json_get_str(in,"key",""));
        else if(!strcmp(func,"runtime_facts_path")) out=w10_runtimepat_34adf5_runtime_facts_path(json_get_str(in,"home_root",""),json_get_str(in,"key",""));
        else if(!strcmp(func,"cli_command_name")) out=w10_runtimepat_34adf5_cli_command_name(json_get_str(in,"command",""));
        else if(!strcmp(func,"shell_exports")) out=w10_runtimepat_34adf5_shell_exports(json_obj_get(in,"env"),json_get_str(in,"dialect","sh"));
        else out=json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}

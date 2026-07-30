/*
 * t_port_billing_usage.c — oracle harness for the billing_usage helpers in
 * src/agent/port_billing_usage.c (ports of agent/billing_usage.py).
 * Reads the fixture from argv[1] (one op per line), emits one JSON object
 * per line. Ops mirror sta_oracle_billing_usage.py:
 *   renews <text>     -> billing_usage_format_renews
 *   model <json>      -> usage_model_from_account (json_t*) + emit dict
 */

#include "billing_usage.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_line_alloc(FILE *fp) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, fp);
    if (n < 0) { free(line); return NULL; }
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
    return line;
}

static void emit_double(double v) {
    /* Mirror Python json float repr: integer-valued -> "X.0", else shortest
     * %.6g form (12.5, 0.06, 0.25, 1.0 are already shortest). */
    if (v == (double)(long long)v) printf("%.1f", v);
    else printf("%.6g", v);
}

static void emit_bar(const usage_bar_t *b, const char *kind) {
    if (!b) { printf("null"); return; }
    int pct = 0;
    bool has_pct = usage_bar_pct_used(b, &pct);
    char pctbuf[16];
    snprintf(pctbuf, sizeof(pctbuf), "%s", has_pct ? "0" : "null");
    if (has_pct) snprintf(pctbuf, sizeof(pctbuf), "%d", pct);
    printf("{\"kind\":\"%s\",\"remaining_usd\":", kind);
    emit_double(usage_bar_remaining(b));
    printf(",\"total_usd\":");
    emit_double(usage_bar_total(b));
    printf(",\"spent_usd\":");
    emit_double(usage_bar_spent(b));
    printf(",\"pct_used\":%s,\"fill_fraction\":", pctbuf);
    emit_double(usage_bar_fill_fraction(b));
    printf("}");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.in>\n", argv[0]); return 2; }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }

    char *line;
    while ((line = read_line_alloc(fp)) != NULL) {
        if (!*line || line[0] == '#') { free(line); continue; }

        char op[32];
        const char *rest = "";
        size_t i = 0;
        while (line[i] == ' ') i++;
        size_t s = i;
        while (line[i] && line[i] != ' ') op[i - s] = line[i], i++;
        op[i - s] = '\0';
        if (line[i] == ' ') rest = line + i + 1;

        if (strcmp(op, "renews") == 0) {
            char *out = billing_usage_format_renews(rest);
            printf("{\"op\":\"renews\",\"in\":%s,\"out\":%s}\n",
                   json_dumps(json_string(rest), 0),
                   out ? json_dumps(json_string(out), 0) : "null");
            free(out);
        } else if (strcmp(op, "model") == 0) {
            json_t *root = json_parse(rest, strlen(rest));
            usage_model_t *m = usage_model_from_account(root);
            if (!m) {
                printf("{\"op\":\"model\",\"out\":null}\n");
            } else {
                printf("{\"op\":\"model\",\"out\":{");
                printf("\"available\":%s,", usage_model_available(m) ? "true" : "false");
                printf("\"status\":\"%s\",", usage_model_status(m));
                printf("\"plan_name\":%s,", usage_model_plan_name(m) ? json_dumps(json_string(usage_model_plan_name(m)), 0) : "null");
                printf("\"renews_at\":%s,", usage_model_renews_at(m) ? json_dumps(json_string(usage_model_renews_at(m)), 0) : "null");
                printf("\"renews_display\":%s,", usage_model_renews_display(m) ? json_dumps(json_string(usage_model_renews_display(m)), 0) : "null");
                printf("\"subscription_remaining_usd\":");
                if (usage_model_has_subscription_remaining(m)) emit_double(usage_model_subscription_remaining(m));
                else printf("null");
                printf(",\"topup_remaining_usd\":");
                if (usage_model_has_topup_remaining(m)) emit_double(usage_model_topup_remaining(m));
                else printf("null");
                printf(",\"total_spendable_usd\":");
                if (usage_model_has_total_spendable(m)) emit_double(usage_model_total_spendable(m));
                else printf("null");
                printf(",\"has_topup\":%s,", usage_model_has_topup(m) ? "true" : "false");
                printf("\"plan_bar\":");
                emit_bar(usage_model_plan_bar(m), "plan");
                printf(",\"topup_bar\":");
                emit_bar(usage_model_topup_bar(m), "topup");
                printf("}}\n");
                usage_model_free(m);
            }
            json_free(root);
        } else {
            printf("{\"op\":\"unknown\",\"raw\":%s}\n", op);
        }
        free(line);
    }
    fclose(fp);
    return 0;
}

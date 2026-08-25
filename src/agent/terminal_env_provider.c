/* terminal_env_provider.c — pluggable terminal execution backend ABC.
 *
 * Port of Python agent/terminal_env_provider.py:TerminalEnvironmentProvider
 * (vtable ABC pattern; abstract slots are NULL until a concrete provider
 * registers). The tep_* dispatch helpers reproduce the Python base-class
 * @property defaults exactly:
 *
 *   display_name   -> name()
 *   description    -> "Run commands in a <display_name> environment."
 *   skip_guards    -> is_container
 *   env_description-> "a <display_name> environment (likely Linux)"
 *   check_reqs     -> is_available()
 *   probe          -> ("ready","") | ("needs_setup", "<dn> is not configured.")
 *   doctor_checks  -> [(is_available(), "<dn> backend",
 *                       "(configured)" | "(not configured — see setup instructions)")]
 */

#include "terminal_env_provider.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Base-class default bodies (@property implementations) ─────────────── */

const char *tep_default_display_name(terminal_env_provider_t *self)
{
    return self && self->name ? self->name(self) : "";
}

const char *tep_default_description(terminal_env_provider_t *self)
{
    /* f"Run commands in a {self.display_name} environment." */
    static _Thread_local char buf[256];
    if (!self) return "";
    snprintf(buf, sizeof(buf),
             "Run commands in a %s environment.", tep_display_name(self));
    return buf;
}

bool tep_default_skip_container_guards(terminal_env_provider_t *self)
{
    /* Defaults to self.is_container. */
    return self ? self->is_container : false;
}

const char *tep_default_env_description(terminal_env_provider_t *self)
{
    /* f"a {self.display_name} environment (likely Linux)" */
    static _Thread_local char buf[256];
    if (!self) return "";
    snprintf(buf, sizeof(buf),
             "a %s environment (likely Linux)", tep_display_name(self));
    return buf;
}

bool tep_default_check_requirements(terminal_env_provider_t *self,
                                    const json_t *config)
{
    (void)config; /* Default defers to is_available(). */
    return self && self->is_available ? self->is_available(self) : false;
}

static char *tep_xstrdup(const char *s)
{
    return s ? strdup(s) : NULL;
}

void tep_default_probe(terminal_env_provider_t *self, tep_probe_t *out)
{
    if (!out) return;
    out->status = NULL;
    out->detail = NULL;
    if (!self || !self->is_available) {
        out->status = strdup("unavailable");
        out->detail = strdup("provider has no availability check");
        return;
    }
    if (self->is_available(self)) {
        out->status = strdup("ready");
        out->detail = strdup("");
    } else {
        out->status = strdup("needs_setup");
        char buf[256];
        snprintf(buf, sizeof(buf), "%s is not configured.",
                 tep_display_name(self));
        out->detail = strdup(buf);
    }
}

tep_doctor_row_t *tep_default_doctor_checks(terminal_env_provider_t *self)
{
    /* [(ok, f"{display_name} backend",
     *     "(configured)" | "(not configured — see setup instructions)")] */
    bool ok = false;
    if (self && self->is_available) {
        /* try/except around is_available — an exception means not ok. */
        ok = self->is_available(self);
    }
    tep_doctor_row_t *row = calloc(2, sizeof(*row));
    if (!row) return NULL;
    row[0].ok = ok;
    char label[192];
    snprintf(label, sizeof(label), "%s backend", tep_display_name(self));
    row[0].label = strdup(label);
    row[0].detail = strdup(ok ? "(configured)"
                              : "(not configured — see setup instructions)");
    row[1].label = NULL; /* NULL-terminated */
    row[1].detail = NULL;
    return row;
}

/* Free a doctor-rows array returned by tep_doctor_checks. */
void tep_doctor_rows_free(tep_doctor_row_t *rows)
{
    if (!rows) return;
    for (tep_doctor_row_t *r = rows; r->label || r->detail; r++) {
        free(r->label);
        free(r->detail);
    }
    free(rows);
}

/* Free a probe result. */
void tep_probe_free(tep_probe_t *p)
{
    if (!p) return;
    free(p->status);
    free(p->detail);
    p->status = p->detail = NULL;
}

/* ── Dispatch helpers (inheritance semantics: slot ?: base default) ────── */

const char *tep_name(terminal_env_provider_t *self)
{
    /* Abstract method: NULL slot == NotImplementedError surface. */
    return (self && self->name) ? self->name(self) : NULL;
}

const char *tep_display_name(terminal_env_provider_t *self)
{
    if (!self) return "";
    return self->display_name ? self->display_name(self)
                              : tep_default_display_name(self);
}

const char *tep_description(terminal_env_provider_t *self)
{
    if (!self) return "";
    return self->description ? self->description(self)
                             : tep_default_description(self);
}

bool tep_skip_container_guards(terminal_env_provider_t *self)
{
    if (!self) return false;
    return self->skip_container_guards_fn
               ? self->skip_container_guards_fn(self)
               : tep_default_skip_container_guards(self);
}

const char *tep_cache_path_base(terminal_env_provider_t *self)
{
    /* Default None → NULL when no translation applies. */
    if (!self || !self->cache_path_base) return NULL;
    return self->cache_path_base(self);
}

const char *const *tep_strip_env_keys(terminal_env_provider_t *self)
{
    /* Default frozenset() → NULL (empty). */
    if (!self || !self->strip_env_keys) return NULL;
    return self->strip_env_keys(self);
}

const char *tep_env_description(terminal_env_provider_t *self)
{
    if (!self) return "";
    return self->env_description ? self->env_description(self)
                                 : tep_default_env_description(self);
}

bool tep_check_requirements(terminal_env_provider_t *self,
                            const json_t *config)
{
    if (!self) return false;
    return self->check_requirements
               ? self->check_requirements(self, config)
               : tep_default_check_requirements(self, config);
}

void tep_probe(terminal_env_provider_t *self, tep_probe_t *out)
{
    if (!out) return;
    out->status = NULL;
    out->detail = NULL;
    if (self && self->probe) {
        self->probe(self, out);
        return;
    }
    tep_default_probe(self, out);
}

tep_doctor_row_t *tep_doctor_checks(terminal_env_provider_t *self)
{
    if (self && self->doctor_checks) return self->doctor_checks(self);
    return tep_default_doctor_checks(self);
}

void *tep_create_environment(terminal_env_provider_t *self,
                             const tep_create_args_t *args)
{
    /* Abstract factory — NULL slot == NotImplementedError. */
    if (!self || !self->create_environment) return NULL;
    tep_create_args_t safe = {0};
    if (args) safe = *args;
    if (!safe.task_id) safe.task_id = "default";
    return self->create_environment(self, &safe);
}

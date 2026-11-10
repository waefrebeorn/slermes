/*
 * slv_stdlib.c -- the table / math / os / io libraries.
 * See slv_internal.h.
 */
#include "slv_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------- table */

static int64_t check_int(slv *L, slv_value v, const char *who, int n) {
    int ok;
    int64_t i = slv_tointeger(v, &ok);
    if (!ok) slv_error(L, "bad argument #%d to '%s' (number expected)", n, who);
    return i;
}

static slv_value t_insert(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 2 || a[0].t != SLV_TABLE) { slv_error(L, "bad argument #1 to 'insert' (table expected)"); return slv_nil(); }
    slv_table *t = a[0].as.tab;
    size_t len = slv_table_len(t);
    if (n == 2) {
        slv_value k; k.t = SLV_INT; k.as.i = (int64_t)len + 1;
        slv_table_set(L, t, k, a[1]);
    } else {
        int64_t pos = check_int(L, a[1], "insert", 2);
        if (pos < 1 || pos > (int64_t)len + 1) { slv_error(L, "bad argument #2 to 'insert' (position out of bounds)"); return slv_nil(); }
        for (int64_t i = (int64_t)len; i >= pos; i--) {
            slv_value k; k.t = SLV_INT; k.as.i = i;
            slv_value v = slv_table_get(L, t, k);
            k.as.i = i + 1;
            slv_table_set(L, t, k, v);
        }
        slv_value k; k.t = SLV_INT; k.as.i = pos;
        slv_table_set(L, t, k, a[2]);
    }
    return slv_nil();
}

static slv_value t_remove(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 1 || a[0].t != SLV_TABLE) { slv_error(L, "bad argument #1 to 'remove' (table expected)"); return slv_nil(); }
    slv_table *t = a[0].as.tab;
    size_t len = slv_table_len(t);
    int64_t pos = (n >= 2) ? check_int(L, a[1], "remove", 2) : (int64_t)len;
    if (len == 0) return slv_nil();
    if (pos < 1 || pos > (int64_t)len) pos = (int64_t)len;
    slv_value k; k.t = SLV_INT; k.as.i = pos;
    slv_value v = slv_table_get(L, t, k);
    for (int64_t i = pos; i < (int64_t)len; i++) {
        slv_value k2; k2.t = SLV_INT; k2.as.i = i + 1;
        slv_value v2 = slv_table_get(L, t, k2);
        k2.as.i = i;
        slv_table_set(L, t, k2, v2);
    }
    slv_value k3; k3.t = SLV_INT; k3.as.i = (int64_t)len;
    slv_table_set(L, t, k3, slv_nil());
    return v;
}

static slv_value t_concat(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 1 || a[0].t != SLV_TABLE) { slv_error(L, "bad argument #1 to 'concat' (table expected)"); return slv_nil(); }
    const char *sep = (n >= 2 && a[1].t == SLV_STR) ? a[1].as.s : "";
    int64_t i = (n >= 3) ? check_int(L, a[2], "concat", 3) : 1;
    int64_t j = (n >= 4) ? check_int(L, a[3], "concat", 4) : (int64_t)slv_table_len(a[0].as.tab);
    char *buf = NULL; size_t len = 0, cap = 0;
    for (int64_t k = i; k <= j; k++) {
        slv_value key; key.t = SLV_INT; key.as.i = k;
        slv_value v = slv_table_get(L, a[0].as.tab, key);
        if (v.t != SLV_STR && v.t != SLV_INT && v.t != SLV_NUM) {
            free(buf);
            slv_error(L, "invalid value (at index %lld) in table for 'concat'", (long long)k);
            return slv_nil();
        }
        const char *s = slv_tostring_intern(L, v);
        size_t sl = strlen(s);
        if (len + sl + strlen(sep) + 1 > cap) { cap = cap * 2 + sl + strlen(sep) + 1; buf = (char *)realloc(buf, cap); }
        memcpy(buf + len, s, sl); len += sl;
        if (k < j) { size_t sepl = strlen(sep); memcpy(buf + len, sep, sepl); len += sepl; }
    }
    if (buf) buf[len] = 0;
    slv_value r = slv_str(slv_intern(L, buf ? buf : "", len));
    free(buf);
    return r;
}

static slv_value t_unpack(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 1 || a[0].t != SLV_TABLE) { slv_error(L, "bad argument #1 to 'unpack' (table expected)"); return slv_nil(); }
    int64_t i = (n >= 2) ? check_int(L, a[1], "unpack", 2) : 1;
    int64_t j = (n >= 3) ? check_int(L, a[2], "unpack", 3) : (int64_t)slv_table_len(a[0].as.tab);
    int cnt = (int)(j - i + 1);
    if (cnt <= 0) return slv_nil();
    if (cnt > 4096) { slv_error(L, "too many results to unpack"); return slv_nil(); }
    slv_value *out = SLV_ALLOCN(slv_value, cnt, L);
    for (int k = 0; k < cnt; k++) {
        slv_value key; key.t = SLV_INT; key.as.i = i + k;
        out[k] = slv_table_get(L, a[0].as.tab, key);
    }
    return slv_multi_value(L, out, cnt);
}

static slv_value t_pack(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    slv_table *t = slv_table_new(L, n, n);
    for (int i = 0; i < n; i++) {
        slv_value k; k.t = SLV_INT; k.as.i = i + 1;
        slv_table_set(L, t, k, a[i]);
    }
    slv_table_set(L, t, slv_str("n"), slv_integer(n));
    slv_value r; r.t = SLV_TABLE; r.as.tab = t;
    return r;
}

static int cmp_less(slv *L, slv_value a, slv_value b) {
    if (a.t == SLV_STR && b.t == SLV_STR) return strcmp(a.as.s, b.as.s) < 0;
    if ((a.t == SLV_INT || a.t == SLV_NUM) && (b.t == SLV_INT || b.t == SLV_NUM)) {
        double x = (a.t == SLV_INT) ? (double)a.as.i : a.as.n;
        double y = (b.t == SLV_INT) ? (double)b.as.i : b.as.n;
        return x < y;
    }
    return 0;
}

static void quicksort(slv *L, slv_value *arr, int lo, int hi, slv_value cmp) {
    while (lo < hi) {
        int i = lo, j = hi;
        slv_value pivot = arr[(lo + hi) / 2];
        while (i <= j) {
            int less;
            if (cmp.t != SLV_NIL) {
                slv_value args[2]; args[0] = arr[i]; args[1] = pivot;
                less = slv_truthy(slv_call(L, cmp, args, 2));
            } else less = cmp_less(L, arr[i], pivot);
            while (less) {
                i++;
                if (cmp.t != SLV_NIL) {
                    slv_value args[2]; args[0] = arr[i]; args[1] = pivot;
                    less = slv_truthy(slv_call(L, cmp, args, 2));
                } else less = cmp_less(L, arr[i], pivot);
            }
            if (cmp.t != SLV_NIL) {
                slv_value args[2]; args[0] = pivot; args[1] = arr[j];
                less = slv_truthy(slv_call(L, cmp, args, 2));
            } else less = cmp_less(L, pivot, arr[j]);
            while (less) {
                j--;
                if (cmp.t != SLV_NIL) {
                    slv_value args[2]; args[0] = pivot; args[1] = arr[j];
                    less = slv_truthy(slv_call(L, cmp, args, 2));
                } else less = cmp_less(L, pivot, arr[j]);
            }
            if (i <= j) { slv_value tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp; i++; j--; }
        }
        if (j - lo < hi - i) { quicksort(L, arr, lo, j, cmp); lo = i; }
        else { quicksort(L, arr, i, hi, cmp); hi = j; }
    }
}

static slv_value t_sort(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 1 || a[0].t != SLV_TABLE) { slv_error(L, "bad argument #1 to 'sort' (table expected)"); return slv_nil(); }
    slv_table *t = a[0].as.tab;
    size_t len = slv_table_len(t);
    if (len < 2) return slv_nil();
    slv_value *arr = SLV_ALLOCN(slv_value, len, L);
    for (size_t i = 0; i < len; i++) {
        slv_value k; k.t = SLV_INT; k.as.i = (int64_t)i + 1;
        arr[i] = slv_table_get(L, t, k);
    }
    slv_value cmp = (n >= 2) ? a[1] : slv_nil();
    quicksort(L, arr, 0, (int)len - 1, cmp);
    for (size_t i = 0; i < len; i++) {
        slv_value k; k.t = SLV_INT; k.as.i = (int64_t)i + 1;
        slv_table_set(L, t, k, arr[i]);
    }
    return slv_nil();
}

void slv_register_table(slv *L) {
    slv_table *t = slv_table_new(L, 0, 8);
#define T(name, fn) { slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = fn; v.as.cf.ud = NULL; \
    slv_table_set(L, t, slv_str(name), v); }
    T("insert", t_insert);
    T("remove", t_remove);
    T("concat", t_concat);
    T("unpack", t_unpack);
    T("pack", t_pack);
    T("sort", t_sort);
#undef T
    slv_value tv; tv.t = SLV_TABLE; tv.as.tab = t;
    slv_set_global(L, "table", tv);
    /* 5.1 compat */
    slv_value g; g.t = SLV_TABLE; g.as.tab = t;
    (void)g;
}

/* ---------------------------------------------------------------- math */

static slv_value m_floor(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    if (a[0].t == SLV_INT) return a[0];
    int ok; double d = slv_tonumber(a[0], &ok);
    if (!ok) return slv_nil();
    return slv_integer((int64_t)floor(d));
}
static slv_value m_ceil(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    if (a[0].t == SLV_INT) return a[0];
    int ok; double d = slv_tonumber(a[0], &ok);
    if (!ok) return slv_nil();
    return slv_integer((int64_t)ceil(d));
}
static slv_value m_abs(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    if (a[0].t == SLV_INT) return slv_integer(a[0].as.i < 0 ? -a[0].as.i : a[0].as.i);
    int ok; double d = slv_tonumber(a[0], &ok);
    if (!ok) return slv_nil();
    return slv_number(fabs(d));
}
static slv_value m_sqrt(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    return slv_number(sqrt(slv_tonumber(a[0], NULL)));
}
static slv_value m_max(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    slv_value best = a[0];
    for (int i = 1; i < n; i++) {
        int swap;
        if (a[i].t == SLV_INT || a[i].t == SLV_NUM) {
            double x = (a[i].t == SLV_INT) ? (double)a[i].as.i : a[i].as.n;
            double y = (best.t == SLV_INT) ? (double)best.as.i : best.as.n;
            swap = x > y;
        } else swap = 0;
        if (swap) best = a[i];
    }
    return best;
}
static slv_value m_min(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    slv_value best = a[0];
    for (int i = 1; i < n; i++) {
        int swap;
        if (a[i].t == SLV_INT || a[i].t == SLV_NUM) {
            double x = (a[i].t == SLV_INT) ? (double)a[i].as.i : a[i].as.n;
            double y = (best.t == SLV_INT) ? (double)best.as.i : best.as.n;
            swap = x < y;
        } else swap = 0;
        if (swap) best = a[i];
    }
    return best;
}
static slv_value m_fmod(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 2) return slv_nil();
    if (a[0].t == SLV_INT && a[1].t == SLV_INT) {
        if (a[1].as.i == 0) { slv_error(L, "attempt to perform 'n%%0'"); return slv_nil(); }
        int64_t r = a[0].as.i % a[1].as.i;
        if (r != 0 && ((r < 0) != (a[1].as.i < 0))) r += a[1].as.i;
        return slv_integer(r);
    }
    return slv_number(fmod(slv_tonumber(a[0], NULL), slv_tonumber(a[1], NULL)));
}
static slv_value m_modf(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    double d = slv_tonumber(a[0], NULL);
    double ip;
    double fp = modf(d, &ip);
    slv_value out[2];
    out[0] = slv_number(ip);
    out[1] = slv_number(fp);
    return slv_multi_value(L, out, 2);
}
static slv_value m_tointeger(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    int ok; int64_t v = slv_tointeger(a[0], &ok);
    return ok ? slv_integer(v) : slv_nil();
}
static slv_value m_type(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    if (a[0].t == SLV_INT) return slv_str("integer");
    if (a[0].t == SLV_NUM) return slv_str("float");
    return slv_nil();
}
static slv_value m_ult(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 2) return slv_nil();
    int ok1, ok2;
    int64_t x = slv_tointeger(a[0], &ok1), y = slv_tointeger(a[1], &ok2);
    if (!ok1 || !ok2) return slv_nil();
    return slv_bool((uint64_t)x < (uint64_t)y);
}
static slv_value m_random(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    /* xoshiro256** */
    uint64_t *s = L->rand_state;
    uint64_t result = (uint64_t)((s[0] + s[1]) & 0xFFFFFFFFFFFFFFFFULL);
    result = (uint64_t)(((result * 5) & 0xFFFFFFFFFFFFFFFFULL) >> 7) * 9;
    uint64_t t = (uint64_t)(s[1] << 17);
    s[1] ^= s[0]; s[0] ^= t; s[0] = (s[0] << 45) | (s[0] >> 19);
    (void)result;
    uint64_t r = s[0] * 5;
    r = ((r >> 7) * 9) & 0xFFFFFFFFFFFFFFFFULL;
    if (n == 0) return slv_number((double)r / 18446744073709551616.0);
    int64_t lo = (n >= 2) ? slv_tointeger(a[1], NULL) : 1;
    int64_t hi = (n >= 1) ? slv_tointeger(a[0], NULL) : 1;
    if (n >= 2) { lo = slv_tointeger(a[1], NULL); hi = slv_tointeger(a[0], NULL); }
    if (lo > hi) { slv_error(L, "bad argument to 'random' (interval is empty)"); return slv_nil(); }
    uint64_t span = (uint64_t)(hi - lo) + 1;
    return slv_integer(lo + (int64_t)(r % span));
}
static slv_value m_randomseed(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    int64_t s = (n >= 1) ? slv_tointeger(a[0], NULL) : 0;
    L->rand_state[0] = (uint64_t)s;
    L->rand_state[1] = (uint64_t)s ^ 0x9E3779B97F4A7C15ULL;
    if (L->rand_state[0] == 0 && L->rand_state[1] == 0) L->rand_state[0] = 1;
    return slv_nil();
}

static slv_value m_trig1(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    return slv_number(((double (*)(double))ud)(slv_tonumber(a[0], NULL)));
}
static slv_value m_pow(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 2) return slv_nil();
    return slv_number(pow(slv_tonumber(a[0], NULL), slv_tonumber(a[1], NULL)));
}
static slv_value m_log(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    if (n < 1) return slv_nil();
    double x = slv_tonumber(a[0], NULL);
    if (n >= 2) {
        double b = slv_tonumber(a[1], NULL);
        return slv_number(log(x) / log(b));
    }
    return slv_number(log(x));
}

void slv_register_math(slv *L) {
    slv_table *t = slv_table_new(L, 0, 24);
#define M(name, fn) { slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = fn; v.as.cf.ud = NULL; \
    slv_table_set(L, t, slv_str(name), v); }
    M("floor", m_floor); M("ceil", m_ceil); M("abs", m_abs); M("sqrt", m_sqrt);
    M("max", m_max); M("min", m_min); M("fmod", m_fmod); M("modf", m_modf);
    M("tointeger", m_tointeger); M("type", m_type); M("ult", m_ult);
    M("random", m_random); M("randomseed", m_randomseed);
    M("pow", m_pow); M("log", m_log);
#undef M
#define T(name, fn) { slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = m_trig1; v.as.cf.ud = (void *)(fn); \
    slv_table_set(L, t, slv_str(name), v); }
    T("sin", sin); T("cos", cos); T("tan", tan);
    T("asin", asin); T("acos", acos); T("exp", exp);
#undef T
    { slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = m_trig1; v.as.cf.ud = (void *)atan;
      slv_table_set(L, t, slv_str("atan"), v); }
    slv_table_set(L, t, slv_str("pi"), slv_number(3.141592653589793));
    slv_table_set(L, t, slv_str("huge"), slv_number(HUGE_VAL));
    slv_table_set(L, t, slv_str("maxinteger"), slv_integer(INT64_MAX));
    slv_table_set(L, t, slv_str("mininteger"), slv_integer(INT64_MIN));
    slv_value tv; tv.t = SLV_TABLE; tv.as.tab = t;
    slv_set_global(L, "math", tv);
}

/* ------------------------------------------------------------------ os */

static slv_value o_time(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n; (void)ud;
    return slv_integer((int64_t)time(NULL));
}
static slv_value o_clock(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n; (void)ud;
    return slv_number((double)clock() / CLOCKS_PER_SEC);
}
static slv_value o_date(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    const char *fmt = (n >= 1 && a[0].t == SLV_STR) ? a[0].as.s : "%c";
    time_t t = (n >= 2) ? (time_t)slv_tointeger(a[1], NULL) : time(NULL);
    struct tm tmv;
    time_t tt = t;
    struct tm *r = localtime(&tt);
    if (r) tmv = *r;
    char buf[512];
    strftime(buf, sizeof buf, fmt, &tmv);
    return slv_str(slv_intern(L, buf, strlen(buf)));
}
static slv_value o_getenv(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    const char *v = getenv(a[0].as.s);
    return v ? slv_str(slv_intern(L, v, strlen(v))) : slv_nil();
}
static slv_value o_exit(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)ud;
    int code = (n >= 1) ? (int)slv_tointeger(a[0], NULL) : 0;
    exit(code);
}
static slv_value o_remove(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    int r = remove(a[0].as.s);
    if (r == 0) return slv_bool(1);
    slv_value out[2]; out[0] = slv_nil(); out[1] = slv_str(a[0].as.s);
    return slv_multi_value(L, out, 2);
}
static slv_value o_rename(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 2) return slv_nil();
    int r = rename(a[0].as.s, a[1].as.s);
    if (r == 0) return slv_bool(1);
    slv_value out[2]; out[0] = slv_nil(); out[1] = slv_str(a[0].as.s);
    return slv_multi_value(L, out, 2);
}
static slv_value o_tmpname(slv *L, slv_value *a, int n, void *ud) {
    (void)L; (void)a; (void)n; (void)ud;
    char buf[64];
    snprintf(buf, sizeof buf, "slv_tmp_%ld", (long)time(NULL));
    return slv_str(slv_intern(L, buf, strlen(buf)));
}

void slv_register_os(slv *L) {
    slv_table *t = slv_table_new(L, 0, 8);
#define O(name, fn) { slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = fn; v.as.cf.ud = NULL; \
    slv_table_set(L, t, slv_str(name), v); }
    O("time", o_time); O("clock", o_clock); O("date", o_date);
    O("getenv", o_getenv); O("exit", o_exit);
    O("remove", o_remove); O("rename", o_rename); O("tmpname", o_tmpname);
#undef O
    slv_value tv; tv.t = SLV_TABLE; tv.as.tab = t;
    slv_set_global(L, "os", tv);
}

/* ------------------------------------------------------------------ io */
/*
 * A file handle is a table { f = <int64 pointer>, closed = bool }. The methods
 * live on a shared metatable keyed by "f", so there is no userdata type.
 */

static FILE *get_file(slv *L, slv_value v, int argn) {
    if (v.t != SLV_TABLE) {
        slv_error(L, "bad argument #%d to 'read' (file expected)", argn);
        return NULL;
    }
    slv_value f = slv_table_get(L, v.as.tab, slv_str("f"));
    if (f.t != SLV_INT) {
        slv_error(L, "bad argument #%d (file expected)", argn);
        return NULL;
    }
    return (FILE *)(intptr_t)f.as.i;
}

static slv_value i_open(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    if (n < 1 || a[0].t != SLV_STR) {
        slv_error(L, "bad argument #1 to 'open' (string expected)");
        return slv_nil();
    }
    const char *mode = (n >= 2 && a[1].t == SLV_STR) ? a[1].as.s : "r";
    FILE *f = fopen(a[0].as.s, mode);
    if (!f) {
        slv_value out[2];
        out[0] = slv_nil();
        out[1] = slv_str(slv_intern(L, "cannot open file", 17));
        return slv_multi_value(L, out, 2);
    }
    slv_table *t = slv_table_new(L, 0, 4);
    slv_table_set(L, t, slv_str("f"), slv_integer((int64_t)(intptr_t)f));
    slv_table_set(L, t, slv_str("closed"), slv_bool(0));
    slv_value r; r.t = SLV_TABLE; r.as.tab = t;
    return r;
}

static slv_value f_close(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    FILE *f = get_file(L, (n >= 1) ? a[0] : slv_nil(), 1);
    if (!f) return slv_nil();
    fclose(f);
    if (n >= 1 && a[0].t == SLV_TABLE)
        slv_table_set(L, a[0].as.tab, slv_str("closed"), slv_bool(1));
    return slv_bool(1);
}

static slv_value f_write(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    FILE *f = get_file(L, (n >= 1) ? a[0] : slv_nil(), 1);
    if (!f) return slv_nil();
    for (int i = 1; i < n; i++) {
        slv_value v = a[i];
        if (v.t == SLV_STR) fputs(v.as.s, f);
        else if (v.t == SLV_INT || v.t == SLV_NUM) fputs(slv_tostring_intern(L, v), f);
    }
    return a[0];
}

static slv_value f_read(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    FILE *f = get_file(L, (n >= 1) ? a[0] : slv_nil(), 1);
    if (!f) return slv_nil();
    const char *fmt = (n >= 2 && a[1].t == SLV_STR) ? a[1].as.s : "l";
    if (fmt[0] == 'n') {   /* read a number */
        double d;
        if (fscanf(f, "%lf", &d) == 1) return slv_number(d);
        return slv_nil();
    }
    if (fmt[0] == 'a') {   /* read the whole file */
        char *buf = NULL; size_t len = 0, cap = 0; int c;
        while ((c = fgetc(f)) != EOF) {
            if (len + 1 >= cap) { cap = cap ? cap * 2 : 256; buf = (char *)realloc(buf, cap); }
            buf[len++] = (char)c;
        }
        slv_value r = slv_str(slv_intern(L, buf ? buf : "", len));
        free(buf);
        return r;
    }
    /* "l" (default) or "L": one line */
    char buf[4096];
    if (!fgets(buf, sizeof buf, f)) return slv_nil();
    size_t l = strlen(buf);
    if (l && buf[l-1] == '\n') buf[--l] = 0;
    return slv_str(slv_intern(L, buf, l));
}

static slv_value f_seek(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    FILE *f = get_file(L, (n >= 1) ? a[0] : slv_nil(), 1);
    if (!f) return slv_nil();
    const char *whence = (n >= 3 && a[2].t == SLV_STR) ? a[2].as.s : "cur";
    int w = SEEK_SET;
    if (whence[0] == 'c') w = SEEK_CUR;
    else if (whence[0] == 'e') w = SEEK_END;
    long off = (n >= 2) ? (long)slv_tointeger(a[1], NULL) : 0;
    if (fseek(f, off, w) != 0) return slv_nil();
    return slv_integer((int64_t)ftell(f));
}

typedef struct { FILE *f; } line_state;

static slv_value iter_lines(slv *L, slv_value *a, int n, void *ud) {
    (void)a; (void)n;
    line_state *st = (line_state *)ud;
    if (!st || !st->f) return slv_nil();
    char buf[4096];
    if (!fgets(buf, sizeof buf, st->f)) { fclose(st->f); st->f = NULL; return slv_nil(); }
    size_t l = strlen(buf);
    if (l && buf[l-1] == '\n') buf[--l] = 0;
    return slv_str(slv_intern(L, buf, l));
}

static slv_value i_lines(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    FILE *f = stdin;
    line_state *st = NULL;
    if (n >= 1 && a[0].t == SLV_STR) {
        f = fopen(a[0].as.s, "r");
        if (!f) { slv_error(L, "cannot open file"); return slv_nil(); }
    }
    st = (line_state *)calloc(1, sizeof *st);
    st->f = f;
    slv_value it; it.t = SLV_CFUNCTION; it.as.cf.f = iter_lines; it.as.cf.ud = st;
    return it;
}

static slv_value i_write(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    for (int i = 0; i < n; i++) {
        slv_value v = a[i];
        if (v.t == SLV_STR) fputs(v.as.s, stdout);
        else if (v.t == SLV_INT || v.t == SLV_NUM) fputs(slv_tostring_intern(L, v), stdout);
    }
    return slv_nil();
}

static slv_value i_read(slv *L, slv_value *a, int n, void *ud) {
    (void)ud;
    char buf[4096];
    if (!fgets(buf, sizeof buf, stdin)) return slv_nil();
    size_t l = strlen(buf);
    if (l && buf[l-1] == '\n') buf[--l] = 0;
    return slv_str(slv_intern(L, buf, l));
}

void slv_register_io(slv *L) {
    slv_table *t = slv_table_new(L, 0, 8);
#define I(name, fn) { slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = fn; v.as.cf.ud = NULL; \
    slv_table_set(L, t, slv_str(name), v); }
    I("write", i_write); I("read", i_read); I("open", i_open); I("lines", i_lines);
#undef I
    slv_value tv; tv.t = SLV_TABLE; tv.as.tab = t;
    slv_set_global(L, "io", tv);

    /* the file metatable: file:read / write / close / seek / lines */
    slv_table *fm = slv_table_new(L, 0, 6);
#define M(name, fn) { slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = fn; v.as.cf.ud = NULL; \
    slv_table_set(L, fm, slv_str(name), v); }
    M("read", f_read); M("write", f_write); M("close", f_close); M("seek", f_seek);
    M("lines", i_lines);
#undef M
    slv_value idx; idx.t = SLV_TABLE; idx.as.tab = fm;
    slv_table_set(L, fm, slv_str("__index"), idx);
}

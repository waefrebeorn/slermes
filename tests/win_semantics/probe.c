/* win_semantics/probe.c — measure the Windows traps that change ANSWERS, not crashes.
 *
 * Every item here is a way a POSIX->Win32 port silently disagrees with the source
 * program while exiting 0: text-mode translation, rename that will not replace,
 * unlink of an open file, errno that never arrives, IANA timezones ignored by
 * _tzset, and a monotonic clock quantised to the scheduler tick.
 *
 * Each check prints PASS / FAIL / INFO and the evidence it measured. Exit code is
 * non-zero when any check FAILs. INFO means "this is a platform fact you must
 * design around" -- it cannot pass or fail.
 *
 * Build/run: scripts/run_win_semantics.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <io.h>
#include <direct.h>
#include <windows.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "port_utils_ports.h" /* util_rename_replace */
#include "datetime.h"         /* datetime_format_tz / datetime_tz_offset */

static int failures;

static void ok(const char *what, const char *detail) {
    printf("PASS %-34s %s\n", what, detail ? detail : "");
}

static void fail(const char *what, const char *detail) {
    printf("FAIL %-34s %s\n", what, detail ? detail : "");
    failures++;
}

static void info(const char *what, const char *detail) {
    printf("INFO %-34s %s\n", what, detail ? detail : "");
}

/* 1. binary round trip: all 256 byte values must survive. */
static void check_binary_roundtrip(const char *dir) {
    char path[512];
    snprintf(path, sizeof path, "%s\\wc_bytes.bin", dir);
    unsigned char out[256], in[256];
    for (int i = 0; i < 256; i++) out[i] = (unsigned char)i;

    FILE *f = fopen(path, "wb");
    if (!f) { fail("binary write", strerror(errno)); return; }
    size_t w = fwrite(out, 1, sizeof out, f);
    fclose(f);
    f = fopen(path, "rb");
    if (!f) { fail("binary read", strerror(errno)); return; }
    size_t r = fread(in, 1, sizeof in, f);
    fclose(f);
    long sz = (long)r;
    if (w == sizeof out && r == sizeof out && memcmp(out, in, sizeof out) == 0)
        ok("binary round trip", "256/256 bytes identical");
    else {
        char d[128];
        snprintf(d, sizeof d, "wrote %zu read %ld identical=%s", w, sz,
                 (r == sizeof out && memcmp(out, in, sizeof out) == 0) ? "yes" : "no");
        fail("binary round trip", d);
    }
    remove(path);

    /* The trap itself: text mode mangles the same bytes. */
    snprintf(path, sizeof path, "%s\\wc_text.txt", dir);
    f = fopen(path, "w");
    if (f) {
        fwrite("a\nb", 1, 3, f);
        fclose(f);
        struct stat st;
        if (stat(path, &st) == 0) {
            char d[128];
            snprintf(d, sizeof d, "wrote 3 bytes in text mode, on disk %ld", (long)st.st_size);
            if (st.st_size == 3) ok("text mode is not translating", "size 3 == 3");
            else info("text mode translation active", d);
        }
        remove(path);
    }
}

/* 2. rename over an existing file: the POSIX "atomic save" idiom. */
static void check_rename_replace(const char *dir) {
    char a[512], b[512], got[64] = {0};
    snprintf(a, sizeof a, "%s\\wc_new.tmp", dir);
    snprintf(b, sizeof b, "%s\\wc_target.txt", dir);
    FILE *f = fopen(b, "w");
    if (f) { fputs("old", f); fclose(f); }
    f = fopen(a, "w");
    if (f) { fputs("new", f); fclose(f); }

    int rc = util_rename_replace(a, b);
    f = fopen(b, "r");
    if (f) { if (!fgets(got, sizeof got, f)) got[0] = 0; fclose(f); }
    if (rc == 0 && strcmp(got, "new") == 0)
        ok("rename replaces existing", "target holds 'new'");
    else {
        char d[160];
        snprintf(d, sizeof d, "rc=%d target='%s' errno=%d", rc, got, errno);
        fail("rename replaces existing", d);
    }
    remove(a);
    remove(b);
}

/* 3. unlink of an open file (POSIX allows it; Windows does not). */
static void check_unlink_open(const char *dir) {
    char path[512];
    snprintf(path, sizeof path, "%s\\wc_open.tmp", dir);
    FILE *f = fopen(path, "wb");
    if (!f) { info("unlink of open file", "could not create probe file"); return; }
    int rc = remove(path);
    int e = errno;
    fclose(f);
    if (rc == 0) {
        info("unlink of open file", "allowed (POSIX-like)");
        remove(path);
    } else {
        char d[128];
        snprintf(d, sizeof d, "denied, errno=%d (%s) -- design around it", e, strerror(e));
        info("unlink of open file", d);
        remove(path);
    }
}

/* 4. errno after a failed Win32-backed operation. */
static void check_errno(const char *dir) {
    char path[512];
    snprintf(path, sizeof path, "%s\\no_such_dir\\file.txt", dir);
    errno = 0;
    FILE *f = fopen(path, "r");
    int e = errno;
    if (f) { fclose(f); info("errno on missing parent", "unexpectedly opened"); return; }
    char d[160];
    snprintf(d, sizeof d, "fopen -> errno=%d (%s), GetLastError=%lu",
             e, strerror(e), GetLastError());
    if (e == 0) fail("errno on missing parent", "errno never set -- branches read stale values");
    else ok("errno on missing parent", d);
}

/* 5. TZ: is an IANA name honoured by _tzset? */
static void check_timezone(void) {
    _putenv_s("TZ", "America/New_York");
    _tzset();
    time_t t = (time_t)1720000000; /* 2024-07-03, EDT expected (-4h) */
    struct tm tmv, gmv;
    localtime_s(&tmv, &t);
    gmtime_s(&gmv, &t);
    /* struct tm has no tm_gmtoff in MSVCRT: the offset is mktime(local) -
     * _mkgmtime(same fields). */
    long off = (long)(mktime(&tmv) - _mkgmtime(&tmv));
    char d[224];
    snprintf(d, sizeof d, "local hour=%d isdst=%d offset=%ld (EDT would be -14400)",
             tmv.tm_hour, tmv.tm_isdst, off);
    if (off == -14400) ok("IANA TZ honoured", d);
    else info("IANA TZ ignored", d);

    /* The port's own fallback idiom is setenv("TZ","UTC") around mktime(). If
     * even that is ignored, every ISO parse is off by the machine offset. */
    _putenv_s("TZ", "UTC");
    _tzset();
    struct tm u;
    localtime_s(&u, &t);
    snprintf(d, sizeof d, "TZ=UTC local hour=%d (UTC hour would be 14)", u.tm_hour);
    if (u.tm_hour == 14) ok("TZ=UTC honoured", d);
    else info("TZ=UTC ignored", d);
    _putenv_s("TZ", "");
    _tzset();
}

/* 6. Monotonic clock resolution. */
static void check_clock(void) {
    LARGE_INTEGER freq, a, b;
    QueryPerformanceFrequency(&freq);
    long long mx = 0, mn = -1;
    for (int i = 0; i < 20; i++) {
        QueryPerformanceCounter(&a);
        Sleep(1);
        QueryPerformanceCounter(&b);
        long long us = (b.QuadPart - a.QuadPart) * 1000000LL / (freq.QuadPart ? freq.QuadPart : 1);
        if (us > mx) mx = us;
        if (mn < 0 || us < mn) mn = us;
    }
    char d[192];
    snprintf(d, sizeof d, "QPC 1ms-sleep delta min=%lldus max=%lldus (QPF %lld Hz)",
             mn, mx, (long long)freq.QuadPart);
    if (mx > 15000) info("clock quantised to the tick", d);
    else ok("sub-tick clock resolution", d);
}

/* 7. Named-zone formatting through the PORT's own datetime library. */
static void check_port_tz(void) {
    static const struct { const char *zone; time_t ts; const char *want; } cases[] = {
        {"America/New_York", 1720000000, "2024-07-03 05:46"},   /* EDT, -4 */
        {"America/New_York", 1704067200, "2023-12-31 19:00"},   /* EST, -5 */
        {"Asia/Tokyo",       1720000000, "2024-07-03 18:46"},   /* +9, no DST */
        {"Asia/Kolkata",     1720000000, "2024-07-03 15:16"},   /* +5:30 */
        {"Australia/Sydney", 1720000000, "2024-07-03 19:46"},   /* AEST, +10 */
        {"Australia/Sydney", 1704067200, "2024-01-01 11:00"},   /* AEDT, +11 */
    };
    int bad = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *got = datetime_format_tz(cases[i].ts, cases[i].zone, "%Y-%m-%d %H:%M");
        if (!got || strcmp(got, cases[i].want) != 0) {
            char d[256];
            snprintf(d, sizeof d, "%s @%lld -> '%s' (want '%s')",
                     cases[i].zone, (long long)cases[i].ts, got ? got : "(null)",
                     cases[i].want);
            info("named-zone format", d);
            bad++;
        }
        free(got);
    }
    char d[128];
    snprintf(d, sizeof d, "%d/%d zone cases exact", 6 - bad, 6);
    if (bad) fail("named-zone formatting", d);
    else ok("named-zone formatting", d);

    int off = datetime_tz_offset("Asia/Kolkata");
    snprintf(d, sizeof d, "Asia/Kolkata offset=%d (want 19800)", off);
    if (off == 19800) ok("named-zone offset", d);
    else fail("named-zone offset", d);
}

int main(void) {
    char dir[512];
    if (!_getcwd(dir, sizeof dir)) { fprintf(stderr, "cwd\n"); return 2; }
    printf("== windows semantics probe (cwd %s)\n", dir);
    check_binary_roundtrip(dir);
    check_rename_replace(dir);
    check_unlink_open(dir);
    check_errno(dir);
    check_timezone();
    check_clock();
    check_port_tz();
    printf("== %d failure(s)\n", failures);
    return failures ? 1 : 0;
}

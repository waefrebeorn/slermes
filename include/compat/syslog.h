/* Slermes Windows compat: <syslog.h> — no syslog daemon. Messages are dropped;
 * the port's own logger is the real sink. */
#ifndef SLERMES_COMPAT_SYSLOG_H
#define SLERMES_COMPAT_SYSLOG_H
#include <stdarg.h>
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_ERROR   LOG_ERR
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7
#define LOG_PID     0x01
#define LOG_CONS    0x02
#define LOG_NDELAY  0x08
#define LOG_USER    (1 << 3)
#define LOG_DAEMON  (3 << 3)
static inline void openlog(const char *id, int opt, int fac) { (void)id; (void)opt; (void)fac; }
static inline void closelog(void) { }
static inline void syslog(int pri, const char *fmt, ...)
{ (void)pri; (void)fmt; }
#endif

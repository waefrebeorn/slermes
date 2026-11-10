/* Slermes Windows compat: <termios.h> — POSIX tty attributes do not exist on Windows.
 * Struct + constants are real so feature detection compiles; the calls fail
 * cleanly, which is what interactive code already handles for non-tty stdin. */
#ifndef SLERMES_COMPAT_TERMIOS_H
#define SLERMES_COMPAT_TERMIOS_H

#include <sys/types.h>

#ifndef NCCS
#define NCCS 32
#endif

typedef unsigned char  cc_t;
typedef unsigned int   speed_t;
typedef unsigned int   tcflag_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t     c_line;
    cc_t     c_cc[NCCS];
    speed_t  c_ispeed;
    speed_t  c_ospeed;
};

/* c_lflag bits used for raw-mode setup */
#define ISIG   0x0001
#define ICANON 0x0002
#define ECHO   0x0008
#define ECHOE  0x0010
#define ECHOK  0x0020
#define ECHONL 0x0040
#define IEXTEN 0x0400

/* c_iflag bits */
#define IGNCR  0x0080
#define ICRNL  0x0100
#define IXON   0x0200
#define IXOFF  0x0400
#define INLCR  0x0040

/* c_oflag bits */
#define OPOST  0x0001

/* c_cflag bits */
#define CS8    0x0030

/* c_cc indices */
#define VTIME 5
#define VMIN  6

/* tcsetattr actions */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

static inline int tcflush(int fd, int queue) { (void)fd; (void)queue; return -1; }
static inline int tcdrain(int fd) { (void)fd; return -1; }
static inline int tcsendbreak(int fd, int dur) { (void)fd; (void)dur; return -1; }
static inline int cfsetispeed(struct termios *t, speed_t s) { (void)t; (void)s; return -1; }
static inline int cfsetospeed(struct termios *t, speed_t s) { (void)t; (void)s; return -1; }

#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#define B9600  9600
#define B19200 19200
#define B38400 38400

static inline int tcgetattr(int fd, struct termios *t)
{
    (void)fd;
    if (t) {
        unsigned char *p = (unsigned char *)t;
        unsigned long i;
        for (i = 0; i < sizeof(*t); i++) p[i] = 0;
    }
    return -1; /* not a POSIX tty */
}

static inline int tcsetattr(int fd, int action, const struct termios *t)
{
    (void)fd; (void)action; (void)t;
    return -1;
}

static inline int cfmakeraw(struct termios *t) { (void)t; return -1; }

#endif /* SLERMES_COMPAT_TERMIOS_H */

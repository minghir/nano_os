#ifndef _TERMIOS_H_
#define _TERMIOS_H_

// Constante și flag-uri dummy pentru a păși fericit codul din Kilo
#define TCSAFLUSH 0
#define BRKINT    0
#define ICRNL     0
#define INPCK     0
#define ISTRIP    0
#define IXON      0
#define OPOST     0
#define CS8       0
#define ECHO      0
#define ICANON    0
#define IEXTEN    0
#define ISIG      0
#define VMIN      0
#define VTIME     1
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

typedef unsigned long tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t     c_cc[32];
};

#endif
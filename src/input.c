#include "input.h"
#include "fb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <linux/input.h>

static struct termios g_old;
static int g_raw = 0;
static int g_ev[8];
static int g_nev;

static int is_keyboard(int fd) {
    unsigned long ev = 0;
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev)), &ev) < 0) return 0;
    if (!(ev & (1u << EV_KEY))) return 0;
    unsigned long keys[KEY_MAX / (8 * sizeof(long)) + 1];
    memset(keys, 0, sizeof(keys));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return 0;
    /* look like a keyboard if it has letter A or F1 */
    int bit_a = KEY_A;
    int bit_f1 = KEY_F1;
    int has_a  = keys[bit_a / (8 * (int)sizeof(long))]  & (1ul << (bit_a % (8 * (int)sizeof(long))));
    int has_f1 = keys[bit_f1 / (8 * (int)sizeof(long))] & (1ul << (bit_f1 % (8 * (int)sizeof(long))));
    return has_a || has_f1;
}

int input_open(void) {
    memset(g_ev, -1, sizeof(g_ev));
    g_nev = 0;
    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &g_old) == 0) {
            struct termios t = g_old;
            t.c_lflag &= (tcflag_t)~(ICANON | ECHO);
            t.c_cc[VMIN] = 0;
            t.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &t);
            g_raw = 1;
        }
        int fl = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);
    }

    DIR *d = opendir("/dev/input");
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) && g_nev < 8) {
        if (strncmp(de->d_name, "event", 5) != 0) continue;
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (!is_keyboard(fd)) { close(fd); continue; }
        g_ev[g_nev++] = fd;
    }
    closedir(d);
    return 0;
}

void input_close(void) {
    if (g_raw) tcsetattr(STDIN_FILENO, TCSANOW, &g_old);
    for (int i = 0; i < g_nev; i++) close(g_ev[i]);
    g_nev = 0;
}

static int map_keycode(int code) {
    switch (code) {
        case KEY_ESC:
        case KEY_Q:        return SSKEY_QUIT;
        case KEY_F1:
        case KEY_TAB:
        case KEY_M:
        case KEY_SPACE:    return SSKEY_MODE;
        case KEY_F2:
        case KEY_P:        return SSKEY_PAUSE;
        case KEY_F3:
        case KEY_H:        return SSKEY_FREEZE;
        case KEY_F4:
        case KEY_C:        return SSKEY_CLEAR;
        case KEY_LEFT:     return SSKEY_LEFT;
        case KEY_RIGHT:    return SSKEY_RIGHT;
        case KEY_UP:       return SSKEY_UP;
        case KEY_DOWN:     return SSKEY_DOWN;
        case KEY_EQUAL:
        case KEY_KPPLUS:   return SSKEY_ZOOM_IN;
        case KEY_MINUS:
        case KEY_KPMINUS:  return SSKEY_ZOOM_OUT;
        case KEY_RIGHTBRACE:
        case KEY_F5:       return SSKEY_GAIN_UP;
        case KEY_LEFTBRACE:
        case KEY_F6:       return SSKEY_GAIN_DN;
        case KEY_DOT:      return SSKEY_LIFE_UP;
        case KEY_COMMA:    return SSKEY_LIFE_DN;
        case KEY_ENTER:
        case KEY_KPENTER:  return SSKEY_ENTER;
        case KEY_O:        return SSKEY_OPEN;
        default:           return SSKEY_NONE;
    }
}

static int map_char(unsigned char ch) {
    switch (ch) {
        case 27:  return SSKEY_QUIT;
        case 'q': case 'Q': return SSKEY_QUIT;
        case ' ': case 'm': case 'M': case '\t': return SSKEY_MODE;
        case 'p': case 'P': return SSKEY_PAUSE;
        case 'h': case 'H': return SSKEY_FREEZE;
        case 'c': case 'C': return SSKEY_CLEAR;
        case '+': case '=': return SSKEY_ZOOM_IN;
        case '-': case '_': return SSKEY_ZOOM_OUT;
        case ']': return SSKEY_GAIN_UP;
        case '[': return SSKEY_GAIN_DN;
        case '.': return SSKEY_LIFE_UP;
        case ',': return SSKEY_LIFE_DN;
        case '\n':
        case '\r': return SSKEY_ENTER;
        case 'o': case 'O': return SSKEY_OPEN;
        default:  return SSKEY_NONE;
    }
}

int input_poll(void) {
    int sk = fb_poll_key();
    if (sk) return sk;

    /* evdev so function keys work even if stdin is not the kbd */
    for (int i = 0; i < g_nev; i++) {
        struct input_event ev;
        while (read(g_ev[i], &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY && ev.value == 1) {
                int k = map_keycode(ev.code);
                if (k) return k;
            }
        }
    }

    unsigned char buf[16];
    int n = (int)read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return SSKEY_NONE;
    if (n >= 3 && buf[0] == 27 && buf[1] == '[') {
        if (buf[2] == 'A') return SSKEY_UP;
        if (buf[2] == 'B') return SSKEY_DOWN;
        if (buf[2] == 'C') return SSKEY_RIGHT;
        if (buf[2] == 'D') return SSKEY_LEFT;
        /* F1-F4: ESC [ 1 1-4 ~  or ESC O P */
    }
    if (n >= 3 && buf[0] == 27 && buf[1] == 'O') {
        if (buf[2] == 'P') return SSKEY_MODE;   /* F1 */
        if (buf[2] == 'Q') return SSKEY_PAUSE;  /* F2 */
        if (buf[2] == 'R') return SSKEY_FREEZE; /* F3 */
        if (buf[2] == 'S') return SSKEY_CLEAR;  /* F4 */
    }
    if (n >= 5 && buf[0] == 27 && buf[1] == '[' && buf[2] == '1' && buf[4] == '~') {
        if (buf[3] == '5') return SSKEY_GAIN_UP; /* F5 */
        if (buf[3] == '7') return SSKEY_GAIN_DN; /* F6 */
    }
    return map_char(buf[n - 1]);
}

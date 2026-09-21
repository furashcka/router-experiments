#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <stdint.h>

/* LCD pins */
#define PIN_RS 18  /* DS7 */
#define PIN_E  19  /* DS3 */
#define PIN_D4 20  /* DS4 */
#define PIN_D5 21  /* DS5 */
#define PIN_D6 12  /* DS6 */
#define PIN_D7 15  /* DS8 */

/* ---------- 5x8 glyph table ---------- */
typedef struct { uint16_t u; uint8_t p[8]; } glyph_t;

static const glyph_t glyphs[] = {
    /* Uppercase Cyrillic letters */
    {0x0410,{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11,0x00}}, /* U+0410 */
    {0x0411,{0x1F,0x10,0x10,0x1E,0x11,0x11,0x1E,0x00}}, /* U+0411 */
    {0x0412,{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E,0x00}}, /* U+0412 */
    {0x0413,{0x1F,0x10,0x10,0x10,0x10,0x10,0x10,0x00}}, /* U+0413 */
    {0x0414,{0x06,0x0A,0x0A,0x0A,0x0A,0x1F,0x11,0x00}}, /* U+0414 */
    {0x0415,{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0x00}}, /* U+0415 */
    {0x0401,{0x0A,0x1F,0x10,0x1E,0x10,0x10,0x1F,0x00}}, /* U+0401 */
    {0x0416,{0x15,0x15,0x15,0x0E,0x15,0x15,0x15,0x00}}, /* U+0416 */
    {0x0417,{0x0E,0x11,0x01,0x06,0x01,0x11,0x0E,0x00}}, /* U+0417 */
    {0x0418,{0x11,0x13,0x15,0x15,0x19,0x11,0x11,0x00}}, /* U+0418 */
    {0x0419,{0x0A,0x04,0x11,0x13,0x15,0x19,0x11,0x00}}, /* U+0419 */
    {0x041A,{0x11,0x12,0x14,0x18,0x14,0x12,0x11,0x00}}, /* U+041A */
    {0x041B,{0x07,0x09,0x09,0x09,0x09,0x09,0x11,0x00}}, /* U+041B */
    {0x041C,{0x11,0x1B,0x15,0x15,0x11,0x11,0x11,0x00}}, /* U+041C */
    {0x041D,{0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0x00}}, /* U+041D */
    {0x041E,{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E,0x00}}, /* U+041E */
    {0x041F,{0x1F,0x11,0x11,0x11,0x11,0x11,0x11,0x00}}, /* U+041F */
    {0x0420,{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0x00}}, /* U+0420 */
    {0x0421,{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E,0x00}}, /* U+0421 */
    {0x0422,{0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0x00}}, /* U+0422 */
    {0x0423,{0x11,0x11,0x11,0x0F,0x01,0x11,0x0E,0x00}}, /* U+0423 */
    {0x0424,{0x04,0x0E,0x15,0x15,0x15,0x0E,0x04,0x00}}, /* U+0424 */
    {0x0425,{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11,0x00}}, /* U+0425 */
    {0x0426,{0x11,0x11,0x11,0x11,0x11,0x1F,0x01,0x00}}, /* U+0426 */
    {0x0427,{0x11,0x11,0x11,0x0F,0x01,0x01,0x01,0x00}}, /* U+0427 */
    {0x0428,{0x15,0x15,0x15,0x15,0x15,0x15,0x1F,0x00}}, /* U+0428 */
    {0x0429,{0x15,0x15,0x15,0x15,0x15,0x1F,0x01,0x00}}, /* U+0429 */
    {0x042A,{0x18,0x08,0x08,0x0E,0x09,0x09,0x0E,0x00}}, /* U+042A */
    {0x042B,{0x11,0x11,0x11,0x1D,0x13,0x13,0x1D,0x00}}, /* U+042B */
    {0x042C,{0x10,0x10,0x10,0x1E,0x11,0x11,0x1E,0x00}}, /* U+042C */
    {0x042D,{0x0E,0x11,0x01,0x07,0x01,0x11,0x0E,0x00}}, /* U+042D */
    {0x042E,{0x12,0x15,0x15,0x1D,0x15,0x15,0x12,0x00}}, /* U+042E */
    {0x042F,{0x0F,0x11,0x11,0x0F,0x05,0x09,0x11,0x00}}, /* U+042F */
    /* Lowercase Cyrillic letters */
    {0x0430,{0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F,0x00}}, /* U+0430 */
    {0x0431,{0x06,0x08,0x1E,0x11,0x11,0x11,0x0E,0x00}}, /* U+0431 */
    {0x0432,{0x00,0x1E,0x11,0x1E,0x11,0x11,0x1E,0x00}}, /* U+0432 */
    {0x0433,{0x00,0x1F,0x10,0x10,0x10,0x10,0x10,0x00}}, /* U+0433 */
    {0x0434,{0x00,0x06,0x0A,0x0A,0x0A,0x1F,0x11,0x00}}, /* U+0434 */
    {0x0435,{0x00,0x0E,0x11,0x1F,0x10,0x11,0x0E,0x00}}, /* U+0435 */
    {0x0451,{0x0A,0x00,0x0E,0x11,0x1F,0x10,0x0E,0x00}}, /* U+0451 */
    {0x0436,{0x00,0x15,0x15,0x0E,0x15,0x15,0x15,0x00}}, /* U+0436 */
    {0x0437,{0x00,0x0E,0x11,0x06,0x01,0x11,0x0E,0x00}}, /* U+0437 */
    {0x0438,{0x00,0x11,0x13,0x15,0x19,0x11,0x11,0x00}}, /* U+0438 */
    {0x0439,{0x0A,0x04,0x00,0x11,0x13,0x15,0x19,0x11}}, /* U+0439 */
    {0x043A,{0x00,0x11,0x12,0x1C,0x12,0x11,0x11,0x00}}, /* U+043A */
    {0x043B,{0x00,0x07,0x09,0x09,0x09,0x09,0x11,0x00}}, /* U+043B */
    {0x043C,{0x00,0x11,0x1B,0x15,0x11,0x11,0x11,0x00}}, /* U+043C */
    {0x043D,{0x00,0x11,0x11,0x1F,0x11,0x11,0x11,0x00}}, /* U+043D */
    {0x043E,{0x00,0x0E,0x11,0x11,0x11,0x11,0x0E,0x00}}, /* U+043E */
    {0x043F,{0x00,0x1F,0x11,0x11,0x11,0x11,0x11,0x00}}, /* U+043F */
    {0x0440,{0x00,0x1E,0x11,0x11,0x1E,0x10,0x10,0x00}}, /* U+0440 */
    {0x0441,{0x00,0x0E,0x11,0x10,0x10,0x11,0x0E,0x00}}, /* U+0441 */
    {0x0442,{0x00,0x1F,0x04,0x04,0x04,0x04,0x04,0x00}}, /* U+0442 */
    {0x0443,{0x00,0x11,0x11,0x11,0x0F,0x01,0x0E,0x00}}, /* U+0443 */
    {0x0444,{0x04,0x04,0x0E,0x15,0x15,0x0E,0x04,0x00}}, /* U+0444 */
    {0x0445,{0x00,0x11,0x11,0x0A,0x04,0x0A,0x11,0x00}}, /* U+0445 */
    {0x0446,{0x00,0x11,0x11,0x11,0x11,0x1F,0x01,0x00}}, /* U+0446 */
    {0x0447,{0x00,0x11,0x11,0x11,0x0F,0x01,0x01,0x00}}, /* U+0447 */
    {0x0448,{0x00,0x15,0x15,0x15,0x15,0x15,0x1F,0x00}}, /* U+0448 */
    {0x0449,{0x00,0x15,0x15,0x15,0x15,0x1F,0x01,0x00}}, /* U+0449 */
    {0x044A,{0x00,0x18,0x08,0x0E,0x09,0x09,0x0E,0x00}}, /* U+044A */
    {0x044B,{0x00,0x11,0x11,0x1D,0x13,0x13,0x1D,0x00}}, /* U+044B */
    {0x044C,{0x00,0x10,0x10,0x1E,0x11,0x11,0x1E,0x00}}, /* U+044C */
    {0x044D,{0x00,0x0E,0x11,0x07,0x01,0x11,0x0E,0x00}}, /* U+044D */
    {0x044E,{0x00,0x12,0x15,0x15,0x1D,0x15,0x12,0x00}}, /* U+044E */
    {0x044F,{0x00,0x0F,0x11,0x0F,0x05,0x09,0x11,0x00}}, /* U+044F */
    /* Special characters */
    {0x00AB,{0x00,0x00,0x0A,0x14,0x14,0x0A,0x00,0x00}}, /* « */
    {0x00BB,{0x00,0x00,0x14,0x0A,0x0A,0x14,0x00,0x00}}, /* » */
};

static const uint8_t qmark[8] = {0x0E,0x11,0x01,0x02,0x04,0x00,0x04,0x00};

static const uint8_t *get_charmap(uint16_t u) {
    for (size_t i = 0; i < sizeof(glyphs)/sizeof(glyphs[0]); i++)
        if (glyphs[i].u == u) return glyphs[i].p;
    return qmark;
}

/* ---------- Low-level LCD access ---------- */
static const int gpio_pins[] = { PIN_RS, PIN_E, PIN_D4, PIN_D5, PIN_D6, PIN_D7 };
static int gpio_fds[6] = {-1, -1, -1, -1, -1, -1};
static volatile sig_atomic_t stop_requested = 0;
static int io_failed = 0;

static void on_signal(int signo) {
    (void)signo;
    stop_requested = 1;
}

static int install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    return sigaction(SIGINT, &sa, NULL) == 0 &&
           sigaction(SIGTERM, &sa, NULL) == 0;
}

static void close_gpio(void) {
    for (size_t i = 0; i < sizeof(gpio_fds) / sizeof(gpio_fds[0]); ++i) {
        if (gpio_fds[i] >= 0) close(gpio_fds[i]);
        gpio_fds[i] = -1;
    }
}

static int open_gpio(void) {
    char path[64];
    for (size_t i = 0; i < sizeof(gpio_pins) / sizeof(gpio_pins[0]); ++i) {
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_pins[i]);
        gpio_fds[i] = open(path, O_WRONLY | O_CLOEXEC);
        if (gpio_fds[i] < 0) {
            perror(path);
            fprintf(stderr, "Unbind and export the GPIO pins first; see README.\n");
            close_gpio();
            return -1;
        }
    }
    return 0;
}

static void set_pin(int gpio, int v) {
    for (size_t i = 0; i < sizeof(gpio_pins) / sizeof(gpio_pins[0]); ++i) {
        if (gpio_pins[i] != gpio) continue;
        if (lseek(gpio_fds[i], 0, SEEK_SET) < 0 ||
            write(gpio_fds[i], v ? "1" : "0", 1) != 1) {
            io_failed = 1;
        }
        return;
    }
    io_failed = 1;
}

static void lcd_nibble(int nib, int rs) {
    set_pin(PIN_RS, rs);
    set_pin(PIN_D4, (nib >> 0) & 1);
    set_pin(PIN_D5, (nib >> 1) & 1);
    set_pin(PIN_D6, (nib >> 2) & 1);
    set_pin(PIN_D7, (nib >> 3) & 1);
    set_pin(PIN_E, 1); usleep(2);
    set_pin(PIN_E, 0); usleep(50);
}
static void lcd_byte(int b, int rs) {
    lcd_nibble((b >> 4) & 0x0F, rs);
    lcd_nibble(b & 0x0F, rs);
}
static void lcd_cmd(int c)  { lcd_byte(c, 0); }
static void lcd_data(int c) { lcd_byte(c, 1); }

static void lcd_create_char(int loc, const uint8_t *map) {
    loc &= 7;
    lcd_cmd(0x40 | (loc << 3));
    for (int i = 0; i < 8; i++) lcd_data(map[i]);
}

static void lcd_init(void) {
    usleep(50000);
    lcd_nibble(3, 0); usleep(5000);
    lcd_nibble(3, 0); usleep(200);
    lcd_nibble(3, 0); usleep(200);
    lcd_nibble(2, 0); usleep(200);
    lcd_cmd(0x28); lcd_cmd(0x0C); lcd_cmd(0x06); lcd_cmd(0x01);
    usleep(2000);
}
static void lcd_gotoxy(int r, int c) {
    lcd_cmd(0x80 | ((r ? 0x40 : 0x00) + c));
}

/* ---------- CGRAM and display control ---------- */
static uint16_t cgram_utf8[8] = {0};
static uint16_t screen_buf[2][16] = {{0}};

static int cgram_find(uint16_t u) {
    for (int i = 0; i < 8; i++) if (cgram_utf8[i] == u) return i;
    return -1;
}

static int latin_analog(uint16_t u) {
    switch (u) {
        case 0x0410: return 'A';
        case 0x0412: return 'B';
        case 0x0415: return 'E';
        case 0x041A: return 'K';
        case 0x041C: return 'M';
        case 0x041D: return 'H';
        case 0x041E: return 'O';
        case 0x0420: return 'P';
        case 0x0421: return 'C';
        case 0x0422: return 'T';
        case 0x0425: return 'X';
        default: return -1;
    }
}

static void redraw_all(void) {
    uint16_t needed[32]; int n = 0;
    for (int r = 0; r < 2; r++) for (int c = 0; c < 16; c++) {
        uint16_t u = screen_buf[r][c];
        if (u < 0x80) continue;
        if (latin_analog(u) >= 0) continue;
        int found = 0;
        for (int i = 0; i < n; i++) if (needed[i] == u) { found = 1; break; }
        if (!found && n < 32) needed[n++] = u;
    }
    int nl = n > 8 ? 8 : n;
    for (int i = 0; i < nl; i++) {
        lcd_create_char(i, get_charmap(needed[i]));
        cgram_utf8[i] = needed[i];
    }
    for (int i = nl; i < 8; i++) cgram_utf8[i] = 0;

    for (int r = 0; r < 2; r++) {
        lcd_gotoxy(r, 0);
        for (int c = 0; c < 16; c++) {
            uint16_t u = screen_buf[r][c];
            if (u == 0) { lcd_data(' '); continue; }
            if (u < 0x80) { lcd_data(u); continue; }
            int la = latin_analog(u);
            if (la >= 0) { lcd_data(la); continue; }
            int s = cgram_find(u);
            lcd_data(s >= 0 ? s : '?');
        }
    }
}

static void put_char(int r, int c, uint16_t u) {
    screen_buf[r][c] = u;
    if (u == 0 || u == ' ') {
        lcd_gotoxy(r, c); lcd_data(' '); return;
    }
    if (u < 0x80) {
        lcd_gotoxy(r, c); lcd_data(u); return;
    }
    int la = latin_analog(u);
    if (la >= 0) { lcd_gotoxy(r, c); lcd_data(la); return; }
    int s = cgram_find(u);
    if (s >= 0) { lcd_gotoxy(r, c); lcd_data(s); return; }
    for (int i = 0; i < 8; i++) if (cgram_utf8[i] == 0) {
        lcd_create_char(i, get_charmap(u));
        cgram_utf8[i] = u;
        lcd_gotoxy(r, c); lcd_data(i); return;
    }
    redraw_all();
}

/* ---------- UTF-8 decoding ---------- */
static int read_utf8(uint16_t *out) {
    unsigned char b1;
    ssize_t count;
    do {
        count = read(STDIN_FILENO, &b1, 1);
    } while (count < 0 && errno == EINTR && !stop_requested);
    if (count != 1) return -1;
    if (b1 < 0x80) { *out = b1; return 1; }
    if ((b1 & 0xE0) == 0xC0) {
        unsigned char b2;
        if (read(STDIN_FILENO, &b2, 1) != 1 || (b2 & 0xC0) != 0x80) {
            *out = '?';
            return 1;
        }
        *out = ((b1 & 0x1F) << 6) | (b2 & 0x3F);
        return 2;
    }
    if ((b1 & 0xF0) == 0xE0) {
        unsigned char b2, b3;
        if (read(STDIN_FILENO, &b2, 1) != 1 || read(STDIN_FILENO, &b3, 1) != 1) {
            return -1;
        }
        *out = '?';
        return 3;
    }
    if ((b1 & 0xF8) == 0xF0) {
        unsigned char tail[3];
        if (read(STDIN_FILENO, tail, sizeof(tail)) != (ssize_t)sizeof(tail)) return -1;
        *out = '?';
        return 4;
    }
    *out = '?';
    return 1;
}

/* ---------- main ---------- */
int main(void) {
    struct termios old_t;
    int terminal_changed = 0;
    int result = 0;

    if (!install_signal_handlers()) {
        perror("sigaction");
        return 1;
    }
    if (open_gpio() < 0) return 1;

    lcd_init();
    lcd_cmd(0x01); usleep(2000);
    if (io_failed) {
        perror("GPIO write");
        close_gpio();
        return 1;
    }

    printf("LCD 1602: type text. Enter: next line/scroll, Ctrl+C: exit.\n");

    struct termios new_t;
    if (tcgetattr(STDIN_FILENO, &old_t) < 0) {
        perror("tcgetattr");
        close_gpio();
        return 1;
    }
    new_t = old_t;
    new_t.c_lflag &= ~(ICANON | ECHO);
    new_t.c_cc[VMIN] = 1; new_t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_t) < 0) {
        perror("tcsetattr");
        close_gpio();
        return 1;
    }
    terminal_changed = 1;

    int row = 0, col = 0;
    int esc = 0;

    while (!stop_requested && !io_failed) {
        uint16_t u;
        int n = read_utf8(&u);
        if (n < 0) break;

        if (esc == 0 && u == 0x1b) { esc = 1; continue; }
        if (esc == 1) { esc = (u == '[') ? 2 : 0; continue; }
        if (esc == 2) { esc = 0; continue; }

        if (u == 3) break;

        if (u == '\r' || u == '\n') {
            putchar('\n'); fflush(stdout);
            if (row == 0) { row = 1; col = 0; }
            else {
                for (int i = 0; i < 16; i++) screen_buf[0][i] = screen_buf[1][i];
                for (int i = 0; i < 16; i++) screen_buf[1][i] = ' ';
                redraw_all();
                col = 0;
            }
            continue;
        }

        /* Backspace can now move across a line boundary. */
        if (u == 0x7f || u == 0x08) {
            if (col > 0) {
                col--;
                put_char(row, col, ' ');
                printf("\b \b"); fflush(stdout);
            } else if (row == 1) {
                row = 0;
                col = 15;
                put_char(row, col, ' ');
                printf("\b \b"); fflush(stdout);
            }
            /* row == 0 && col == 0: start of buffer, nothing to do. */
            continue;
        }

        if (u < 0x20) continue;

        put_char(row, col, u);
        col++;

        if (u < 0x80) putchar(u);
        else if (u < 0x800) { putchar(0xC0 | (u >> 6)); putchar(0x80 | (u & 0x3F)); }
        else { putchar(0xE0 | (u >> 12)); putchar(0x80 | ((u >> 6) & 0x3F)); putchar(0x80 | (u & 0x3F)); }
        fflush(stdout);

        if (col >= 16) {
            col = 0;
            if (row == 0) row = 1;
            else {
                for (int i = 0; i < 16; i++) screen_buf[0][i] = screen_buf[1][i];
                for (int i = 0; i < 16; i++) screen_buf[1][i] = ' ';
                redraw_all();
            }
        }
    }

    if (io_failed) {
        perror("GPIO write");
        result = 1;
    }
    if (!io_failed) {
        lcd_cmd(0x01);
        usleep(2000);
    }
    set_pin(PIN_E, 0);
    if (terminal_changed) (void)tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
    close_gpio();
    printf("\nExiting.\n");
    return result;
}

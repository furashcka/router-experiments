#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static const char *const gpio_paths[] = {
    "/sys/class/gpio/gpio20/value", /* IN1: DS4 */
    "/sys/class/gpio/gpio21/value", /* IN2: DS5 */
    "/sys/class/gpio/gpio12/value", /* IN3: DS6 */
    "/sys/class/gpio/gpio15/value", /* IN4: DS8 */
};

static const unsigned char half_step[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1},
};

static int gpio_fds[4] = {-1, -1, -1, -1};
static volatile sig_atomic_t stop_requested = 0;

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

static int gpio_write(size_t index, int value) {
    if (lseek(gpio_fds[index], 0, SEEK_SET) < 0) return -1;
    return write(gpio_fds[index], value ? "1" : "0", 1) == 1 ? 0 : -1;
}

static int apply_phase(int phase) {
    for (size_t i = 0; i < 4; ++i) {
        if (gpio_write(i, half_step[phase][i]) < 0) return -1;
    }
    return 0;
}

static void all_off(void) {
    for (size_t i = 0; i < 4; ++i) {
        if (gpio_fds[i] >= 0) (void)gpio_write(i, 0);
    }
}

static void close_gpio(void) {
    for (size_t i = 0; i < 4; ++i) {
        if (gpio_fds[i] >= 0) close(gpio_fds[i]);
        gpio_fds[i] = -1;
    }
}

static int open_gpio(void) {
    for (size_t i = 0; i < 4; ++i) {
        gpio_fds[i] = open(gpio_paths[i], O_WRONLY | O_CLOEXEC);
        if (gpio_fds[i] < 0) {
            perror(gpio_paths[i]);
            fprintf(stderr, "Unbind and export the GPIO pins first; see README.\n");
            close_gpio();
            return -1;
        }
    }
    return 0;
}

static int read_byte_timeout(int timeout_ms) {
    struct pollfd descriptor = {.fd = STDIN_FILENO, .events = POLLIN};
    int ready;
    do {
        ready = poll(&descriptor, 1, timeout_ms);
    } while (ready < 0 && errno == EINTR && !stop_requested);

    if (ready <= 0) return -1;
    unsigned char byte;
    return read(STDIN_FILENO, &byte, 1) == 1 ? byte : -1;
}

static void sleep_us(long microseconds) {
    struct timespec delay = {
        .tv_sec = microseconds / 1000000,
        .tv_nsec = (microseconds % 1000000) * 1000L,
    };
    while (!stop_requested && nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
}

static int parse_delay(const char *text, long *delay_us) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 500 || value > 1000000) {
        return -1;
    }
    *delay_us = value;
    return 0;
}

static void usage(const char *program) {
    printf("Usage: %s [delay_us]\n", program);
    printf("Range: 500..1000000 us; default: 2000 us per half-step.\n");
}

int main(int argc, char **argv) {
    long delay_us = 2000;
    struct termios old_termios;
    int terminal_changed = 0;
    int phase = 0;
    int direction = 0;
    int result = 0;

    if (argc > 2 || (argc > 1 && strcmp(argv[1], "--help") == 0)) {
        usage(argv[0]);
        return argc > 2 ? 1 : 0;
    }
    if (argc == 2 && parse_delay(argv[1], &delay_us) < 0) {
        fprintf(stderr, "Invalid delay: %s\n", argv[1]);
        return 1;
    }
    if (!install_signal_handlers()) {
        perror("sigaction");
        return 1;
    }
    if (open_gpio() < 0) return 1;
    all_off();

    if (tcgetattr(STDIN_FILENO, &old_termios) < 0) {
        perror("tcgetattr");
        result = 1;
        goto cleanup;
    }
    struct termios raw = old_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
        perror("tcsetattr");
        result = 1;
        goto cleanup;
    }
    terminal_changed = 1;

    printf("28BYJ-48 + ULN2003, delay: %ld us.\n", delay_us);
    printf("Left/Right: rotate, Up/Down: stop, Ctrl+C: exit.\n");

    while (!stop_requested) {
        int byte = read_byte_timeout(0);
        if (byte == 0x1b && read_byte_timeout(30) == '[') {
            int arrow = read_byte_timeout(30);
            if (arrow == 'D' || arrow == 'C') {
                direction = arrow == 'D' ? 1 : -1;
                printf("\r%s ", direction > 0 ? "LEFT " : "RIGHT");
                fflush(stdout);
            } else if (arrow == 'A' || arrow == 'B') {
                direction = 0;
                all_off();
                printf("\rSTOP     ");
                fflush(stdout);
            }
        }

        if (direction == 0) {
            sleep_us(20000);
            continue;
        }

        phase = (phase + direction + 8) % 8;
        if (apply_phase(phase) < 0) {
            perror("GPIO write");
            result = 1;
            break;
        }
        sleep_us(delay_us);
    }

cleanup:
    all_off();
    if (terminal_changed) (void)tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    close_gpio();
    printf("\nExiting. All coils are off.\n");
    return result;
}

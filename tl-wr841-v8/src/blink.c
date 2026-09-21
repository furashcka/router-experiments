#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LED_DIR "/sys/class/leds/tp-link:green:wlan"
#define BRIGHTNESS_PATH LED_DIR "/brightness"
#define TRIGGER_PATH LED_DIR "/trigger"

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

static int write_text(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    size_t left = strlen(text);
    const char *cursor = text;
    while (left > 0) {
        ssize_t written = write(fd, cursor, left);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            perror(path);
            close(fd);
            return -1;
        }
        cursor += written;
        left -= (size_t)written;
    }

    if (close(fd) < 0) {
        perror(path);
        return -1;
    }
    return 0;
}

static int read_text(const char *path, char *buffer, size_t size) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    ssize_t count;
    do {
        count = read(fd, buffer, size - 1);
    } while (count < 0 && errno == EINTR);

    if (count < 0) perror(path);
    close(fd);
    if (count < 0) return -1;

    buffer[count] = '\0';
    while (count > 0 && (buffer[count - 1] == '\n' || buffer[count - 1] == '\r')) {
        buffer[--count] = '\0';
    }
    return 0;
}

static void selected_trigger(const char *trigger_list, char *result, size_t size) {
    const char *begin = strchr(trigger_list, '[');
    const char *end = begin ? strchr(begin, ']') : NULL;

    result[0] = '\0';
    if (!begin || !end || end <= begin + 1) return;

    size_t length = (size_t)(end - begin - 1);
    if (length >= size) length = size - 1;
    memcpy(result, begin + 1, length);
    result[length] = '\0';
}

static int sleep_ms(long milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };

    while (!stop_requested && nanosleep(&delay, &delay) < 0) {
        if (errno != EINTR) return -1;
    }
    return 0;
}

static int parse_positive(const char *text, long min, long max, long *value) {
    char *end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < min || parsed > max) {
        return -1;
    }
    *value = parsed;
    return 0;
}

static void usage(const char *program) {
    printf("Usage: %s [count] [half_period_ms]\n", program);
    printf("Default: 10 blinks with a 300 ms half-period.\n");
}

int main(int argc, char **argv) {
    long count = 10;
    long period_ms = 300;
    char old_brightness[32];
    char trigger_list[512];
    char old_trigger[64];
    int trigger_changed = 0;
    int result = 0;

    if (argc > 3 || (argc > 1 && strcmp(argv[1], "--help") == 0)) {
        usage(argv[0]);
        return argc > 3 ? 1 : 0;
    }
    if (argc > 1 && parse_positive(argv[1], 1, 1000000, &count) < 0) {
        fprintf(stderr, "Invalid blink count: %s\n", argv[1]);
        return 1;
    }
    if (argc > 2 && parse_positive(argv[2], 1, 60000, &period_ms) < 0) {
        fprintf(stderr, "Invalid period: %s\n", argv[2]);
        return 1;
    }
    if (!install_signal_handlers()) {
        perror("sigaction");
        return 1;
    }
    if (read_text(BRIGHTNESS_PATH, old_brightness, sizeof(old_brightness)) < 0 ||
        read_text(TRIGGER_PATH, trigger_list, sizeof(trigger_list)) < 0) {
        fprintf(stderr, "WLAN LED (DS2) was not found. OpenWrt 19.07 on a TL-WR841N/ND v8 is expected.\n");
        return 1;
    }

    selected_trigger(trigger_list, old_trigger, sizeof(old_trigger));
    if (write_text(TRIGGER_PATH, "none") < 0) return 1;
    trigger_changed = 1;

    printf("Blinking DS2/WLAN, the first LED after POWER (%ld times).\n", count);
    for (long i = 0; i < count && !stop_requested; ++i) {
        if (write_text(BRIGHTNESS_PATH, "1") < 0 || sleep_ms(period_ms) < 0 ||
            write_text(BRIGHTNESS_PATH, "0") < 0 || sleep_ms(period_ms) < 0) {
            result = 1;
            break;
        }
    }

    if (write_text(BRIGHTNESS_PATH, old_brightness) < 0) result = 1;
    if (trigger_changed && old_trigger[0] != '\0' &&
        write_text(TRIGGER_PATH, old_trigger) < 0) {
        result = 1;
    }

    printf(stop_requested ? "Stopped.\n" : "Done.\n");
    return result;
}

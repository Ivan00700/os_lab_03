#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>

#define BUF_SIZE 1024

typedef struct {
    volatile sig_atomic_t child_ready;
    volatile sig_atomic_t has_request;
    volatile sig_atomic_t has_response;
    char text[BUF_SIZE];
    int is_valid;
} shm_data_t;

volatile sig_atomic_t got_response = 0;

void sigusr2_handler(int sig) {
    got_response = 1;
}

static ssize_t read_line(int fd, char *buf, size_t max_len) {
    if (max_len == 0) return -1;

    size_t i = 0;
    while (i + 1 < max_len) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n == 0) {
            break; // EOF
        }
        if (n < 0) {
            return -1;
        }

        if (c == '\n') {
            break;
        }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

int main() {
    char filename[256];

    write(1, "Enter output filename:\n", 24);
    ssize_t n = read_line(0, filename, sizeof(filename));

    if (n <= 0) {
        write(2, "No filename provided\n", 21);
        return 1;
    }

    (void)n;

    int fd = open("shared.bin", O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) { perror("open"); return 1; }

    if (ftruncate(fd, sizeof(shm_data_t)) == -1) {
        perror("ftruncate");
        return 1;
    }

    shm_data_t *data = mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    close(fd);

    data->child_ready = 0;
    data->has_request = 0;
    data->has_response = 0;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        execlp("./child", "./child", NULL);
        perror("execlp");
        _exit(1);
    }

    signal(SIGUSR2, sigusr2_handler);

    while (!data->child_ready) {
        usleep(1000);
    }

    int out_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) { perror("open output"); return 1; }

    char buf[BUF_SIZE];
    ssize_t m;

    while (1) {
        write(1, "Enter string (Ctrl+D to exit):\n", 32);
        m = read_line(0, buf, sizeof(buf));
        if (m == 0) break; // EOF
        if (m < 0) { perror("read"); break; }

        strncpy(data->text, buf, sizeof(data->text) - 1);
        data->text[sizeof(data->text) - 1] = '\0';
        data->has_request = 1;
        data->has_response = 0;
        got_response = 0;

        kill(pid, SIGUSR1);

        while (!got_response) pause();

        if (data->is_valid) {
            write(out_fd, data->text, strlen(data->text));
            write(out_fd, "\n", 1);
        } else {
            write(1, "Invalid string: ", 16);
            write(1, data->text, strlen(data->text));
            write(1, "\n", 1);
        }
    }

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);

    munmap(data, sizeof(shm_data_t));
    close(out_fd);

    return 0;
}

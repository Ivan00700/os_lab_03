#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE 1024

// Структура в общей области памяти
typedef struct {
    volatile sig_atomic_t child_ready;
    volatile sig_atomic_t has_request;
    volatile sig_atomic_t has_response;
    char text[BUF_SIZE];
    int is_valid;
} shm_data_t;

shm_data_t *data;

// Обработчик процесса
void sigusr1_handler(int sig) {
    if (!data->has_request) return;

    char *s = data->text;
    size_t len = strlen(s);
    // проверка строки согласно заданию варианта
    if (len > 0 && (s[len-1] == '.' || s[len-1] == ';'))
        data->is_valid = 1;
    else
        data->is_valid = 0;

    data->has_request = 0;
    data->has_response = 1; // Ставим has_response=1, т.к. обработали строку и готовы посылать сигнал родителю
    // Отправка сигнала родителю о том, что строка обработана согласно заданию
    kill(getppid(), SIGUSR2);
}

int main() {
    // Открываем и mmap'им тот же файл
    int fd = open("shared.bin", O_RDWR, 0600);
    if (fd < 0) {
        perror("open shared");
        return 1;
    }

    data = mmap(NULL, sizeof(shm_data_t),
                PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);

    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    close(fd);

    signal(SIGUSR1, sigusr1_handler);

    data->child_ready = 1;

    // Ребёнок "спит" пока не приходят ему сигналы и "просыпается" как только таковые получает
    while (1) pause();
}

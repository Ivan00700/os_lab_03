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

// Итак, ВСЕ-ВСЕ(ВСЕ) комменнты здесь написаны чисто мной, все очепятки мои, горжусь ими
// P.S. не, если нужны офф коменты я могу и офф коменты писать, попробую например на дочернем файле, но так менее интересно :/



// структура в области памяти для обоих: родителя и его дитя, через нее они обмениваются инфой
typedef struct {
    volatile sig_atomic_t has_request; // родитель после записи строки ставит сюда 1 и посылает сигнал ребенку
    volatile sig_atomic_t has_response; // дите после того как сделало вещи, ставит в респонз 1 и посылает ответный сигнал родителю
    char text[BUF_SIZE]; // сюда родитель записывает строку которую нужно обработать чилдрену
    int is_valid; // прошла строка проверку или нет
} shm_data_t;

volatile sig_atomic_t got_response = 0; // глобальная переменная сигнализирующая основному циклу родителя о том что пришел сигнал

// обработчик сигнала, т.е. при получении сигнала сигуср2 автоматически вызывается этот обработчик
void sigusr2_handler(int sig) {
    got_response = 1; // нужно чтобы родитель ждал пока ребенок проверит строку 
}

int main() {
    char filename[256]; // ну очев

    write(1, "Enter output filename:\n", 24);
    ssize_t n = read(0, filename, sizeof(filename));
    // if (n <= 0) { // будем считать пользователь не лох
    //     write(2, "Filename required\n", 18);
    //     return 1;
    // }
    if (filename[n-1] == '\n') filename[n-1] = '\0';

    // создаем как раз таки этот общий файл и задаем его размер
    // на вход: Имя требуемого файла | флаги | цифры прав доступа
    // O_RDWR - ридВрайт на чтение и запись, O_CREAT - если нету файла создаст новый, O_TRUNC - если такового нету то создаст новый
    int fd = open("shared.bin", O_RDWR | O_CREAT | O_TRUNC, 0600); //дскрптр
    if (fd < 0) { perror("open"); return 1; }

    // трунКейт укорачивает файл до размера структуры, иначебы ммап зафакапился потому что memory-mapped файл должен иметь размер не меньше, чем отображаемая память.
    if (ftruncate(fd, sizeof(shm_data_t)) == -1) {
        perror("ftruncate");
        return 1;
    }
    // отображаем файл в память
    // что на вход? - адрес куда пихать(обычно нулл) | длина области отображения | права доступа | флаги, есть еще мап_приват где нельзя вносить изменения в файл |
    // | дескриптор файловый | смещение в файле(0 обычно - значит полностью будем отображать)
    shm_data_t *data = mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    close(fd);

    data->has_request = 0;
    data->has_response = 0;

    // форк ребенка
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

    // установка сигнала, когда ребенок закончит он выставит got_response = 1
    signal(SIGUSR2, sigusr2_handler);

    // открываем файл на запись резов
    int out_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) { perror("open output"); return 1; }

    char buf[BUF_SIZE];
    ssize_t m;

    while (1) {
        write(1, "Enter string (Ctrl+D to exit):\n", 32);
        m = read(0, buf, sizeof(buf));
        if (m == 0) break; // EOF
        if (m < 0) { perror("read"); break; }

        if (buf[m-1] == '\n') buf[m-1] = '\0';

        // 
        strcpy(data->text, buf); // копирует текст в mmap область
        data->has_request = 1; // сигнализирует о том что есть строка ребенку на обработку
        data->has_response = 0; // ответа нема
        got_response = 0;

        // уведомляем ребенка что для него есть работенка
        kill(pid, SIGUSR1);

        // далее мы ждем пока не получим (SIGUSR2) респонз от ребенка с его готовой работой
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

    kill(pid, SIGTERM); // Сигнал Терминэйт, чтобы тот не висел зазря
    waitpid(pid, NULL, 0);

    munmap(data, sizeof(shm_data_t)); // отсоединяет созданное ранее отображение памяти от процесса
    close(out_fd);

    return 0;
} // все, все написал, спок

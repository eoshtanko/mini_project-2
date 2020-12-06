#include <pthread.h>
#include <semaphore.h>
#include <cstdlib>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <ctime>

// Штанько Екатерина Олеговна БПИ193 (Вариант 27) -> Вариант 5
// (так как всего предложено 22 задачи)

// 5. Задача о каннибалах.
// Племя из n дикарей ест вместе из большого горшка, который вмещает m кусков тушеного миссионера.
// Когда дикарь хочет обедать, он ест из горшка один кусок, если только горшок не пуст,
// иначе дикарь будит повара и ждет, пока тот не наполнит горшок.
// Повар, сварив обед, засыпает.
// Создать многопоточное приложение, моделирующее обед дикарей.
// При решении задачи пользоваться семафорами.

// Аргументы для запуска приложения из командной строки:
// 1. argv[1] - кол-во дикарей в племени(n).
// 2. argv[2] - вместимость горшка(m).
// 3. argv[3] - то, на сколько порций(полных горшков) хватит несчастного миссионера.

// Вместимость горшка(буфера)
int m;

// Кол-во дикарей в племени
int n;

// То, на сколько порций(полных горшков) хватит несчастного миссионера
// (сколько раз будет происходить заполнение горшка)
int numberOfServings;

// Куски тушеного миссионера в горшке(буфер)
int body;

// Семафор empty будем использовать для организации ожидания
// повара при заполненном горшке
sem_t *empty;

// Семафор full будем использовать для гарантии того,
// что каннибал будет ждать, пока в горшке появится мясо.
sem_t *full;

// Для организации взаимоисключения на критических участках
pthread_mutex_t mutexFirst;
pthread_mutex_t mutexSecond;

// Инициализатор генератора случайных чисел
unsigned int seed = 101;

// счетчики
static int i = 0, j = 0;
// вспомогательная переменная для корректной остановки
bool stop = true;

// В задаче всего один продьюсер - повар
void *Producer(void *param) {
    while (i < numberOfServings) {
        pthread_mutex_lock(&mutexSecond);
        // Уменьшаем значение семафора
        // Если значение семафора равно нулю, то вызов блокируется до тех пор, пока не станет возможным выполнить уменьшение,
        // (пока не произойдет вызов sem_post каннибалом)
        sem_wait(empty);
        printf("Время: %.3lf. Повар приготовил тушеного миссионера! Теперь в горшочке целых %d лакомых кусочков.\n",
               clock() / 1000.0, m);
        body = m;  // Заполняем горшок(put item)
        for (int i = 0; i < m; i++) {
            // Эта функция увеличивает значение семафора и разблокирует ожидающие потоки(каннибалов)
            sem_post(full);
        }
        i++;
        pthread_mutex_unlock(&mutexSecond);
    }
    sem_close(empty);
    // Когда все процессы закончили использовать семафор, мы его удаляем из системы
    sem_unlink("emptySem");
    return nullptr;
}

// В задаче n консьюмеров - каннибалы
void *Consumer(void *param) {
    int cNum = *((int *) param); // номер канибала
    while (stop) {
        // время проголодаться(от 0 до 15 сек)
        sleep(rand() % 16);
        pthread_mutex_lock(&mutexFirst);
        if (j < numberOfServings * m) {
            // Уменьшаем значение семафора
            // Если значение семафора равно нулю, то вызов блокируется до тех пор, пока не станет возможным выполнить уменьшение,
            // (пока не произойдет вызов sem_post поваром)
            sem_wait(full);
            printf("Время: %.3lf. Каннибал #%d съел кусок. Осталось: %d кусков.\n", clock() / 1000.0, cNum + 1,
                   body - 1);
            // Уменьшаем число кусков в горшке(get item)
            body--;
            // Если еда закончилась
            if (body == 0) {
                // Эта функция увеличивает значение семафора и разблокирует ожидающий поток(повора)
                sem_post(empty);
            }
            j++;
        } else {
            stop = false;
        }
        pthread_mutex_unlock(&mutexFirst);
    }
    sem_close(full);
    // Когда все процессы закончили использовать семафор, мы его удаляем из системы
    sem_unlink("fullSem");
    return nullptr;
}

/// Методод для проверки данных, введенных через командную строку
void checkData(int argc, char *argv[]) {
    if (argc != 4) {
        throw std::invalid_argument("Ошибка в данных переданных через командную строку!");
    }
    n = atoi(argv[1]);
    if (n < 1) {
        throw std::invalid_argument("Число каннибалов не может быть меньше 1!");
    }
    m = atoi(argv[2]);
    if (m < 1) {
        throw std::invalid_argument("Что это за горшок, в который и один кусок не помещается!");
    }
    numberOfServings = atoi(argv[3]);
    if (numberOfServings < 1) {
        throw std::invalid_argument("Если Вы ни разу не накормите каннибалов - они съедят Вас!");
    }
}

int main(int argc, char *argv[]) {
    try {
        // Если программа была завершена некорректно, и семафоры не были удалены
        // удаляем их
        sem_unlink("emptySem");
        sem_unlink("fullSem");

        checkData(argc, argv);
        srand(seed);

        pthread_mutex_init(&mutexSecond, nullptr);
        pthread_mutex_init(&mutexFirst, nullptr);

        // Неименованные семафоры не поддерживаются на mac, поэтому мне пришлось
        // использовать именнованные(named).
        empty = sem_open("emptySem", O_CREAT, 0600, 1);
        full = sem_open("fullSem", O_CREAT, 0600, 0);

        // создаем повара
        pthread_t threadP;
        pthread_create(&threadP, nullptr, Producer, (void *) nullptr);

        // создаем каннибалов
        // учитываем наличие основного потока
        pthread_t threadC[n - 1];
        int consumers[n - 1];

        for (int i = 0; i < n - 1; i++) {
            consumers[i] = i + 1;
            pthread_create(&threadC[i], nullptr, Consumer, (void *) (consumers + i));
        }
        int mNum = 0;
        Consumer((void *) &mNum);
    }
    catch (const std::exception &e) {
        std::cout << e.what();
    }
    return 0;
}

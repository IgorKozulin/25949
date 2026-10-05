#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern char *tzname[2];

int main(void) {
    // 1. Устанавливаем часовой пояс Калифорнии
    if (setenv("TZ", "America/Los_Angeles", 1) != 0) {
        perror("setenv");
        return 1;
    }

    // 2. Применяем настройки нового часового пояса
    tzset();

    // 3. Получаем текущее системное время (UTC)
    time_t now;
    if (time(&now) == (time_t)-1) {
        perror("time");
        return 1;
    }

    // 4. Преобразуем его в локальное время Калифорнии
    struct tm *sp = localtime(&now);
    if (sp == NULL) {
        perror("localtime");
        return 1;
    }

    // 5. Выводим дату и время
    // Если действует летнее время (tm_isdst > 0), то берем tzname[1] (PDT), иначе tzname[0] (PST)
    int dst_index = (sp->tm_isdst > 0) ? 1 : 0;

    printf("%02d/%02d/%04d %02d:%02d %s\n",
           sp->tm_mon + 1,       // месяцы от 0 до 11 (+1)
           sp->tm_mday,          // день
           sp->tm_year + 1900,   // год от 1900 (+1900)
           sp->tm_hour,          // часы
           sp->tm_min,           // минуты
           tzname[dst_index]);   // название пояса (PST или PDT)

    return 0;
}

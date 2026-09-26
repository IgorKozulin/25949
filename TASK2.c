#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

extern char *tzname[];

int main(void)
{
    time_t now;
    struct tm *sp;

    //PST (Pacific Standard Time)
    //PDT (Pacific Daylight Time)
    if (putenv("TZ=PST8PDT") != 0) {
        perror("putenv failed");
        exit(1);
    }
    tzset();
    (void) time(&now);
    printf("%s", ctime(&now));

    sp = localtime(&now);
    printf("%d/%d/%02d %d:%02d %s\n",
        sp->tm_mon + 1, sp->tm_mday,
        sp->tm_year, sp->tm_hour,
        sp->tm_min, tzname[sp->tm_isdst]);

    exit(0);
}
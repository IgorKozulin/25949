#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define MAX_LINE 40
#define BUFFER_SIZE 4096

static struct termios old_term;

static int previous_lines = 1;
static int cursor_pos = 0;
static int cursor_screen_line = 0;
static int desired_column = -1;

static int screen_rows[BUFFER_SIZE + 1];
static int screen_cols[BUFFER_SIZE + 1];

void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
}

void enable_raw_mode(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &old_term);
    atexit(disable_raw_mode);
    raw = old_term;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void bell(void)
{
    putchar('\a');
    fflush(stdout);
}

/*
 * Очищаем предыдущий вывод редактора.
 * Работаем относительно текущей строки курсора.
 */
void clear_previous_output(void)
{
    int i;
    printf("\r");

    for (i = 0; i < cursor_screen_line; i++)
        printf("\033[A");

    for (i = 0; i < previous_lines; i++)
    {
        printf("\033[2K");

        if (i < previous_lines - 1)
            printf("\033[B");
    }

    for (i = 1; i < previous_lines; i++)
        printf("\033[A");

    printf("\r");

    cursor_screen_line = 0;
}

/*
 * Рассчитываем положение каждой позиции буфера.
 * При draw != 0 одновременно выводим текст.
 *
 * Переносы слов и положение курсора рассчитываются
 * одним алгоритмом.
 */
void build_layout(const char *buffer, int len, int draw)
{
    int i = 0;
    int row = 0;
    int col = 2;

    screen_rows[0] = 0;
    screen_cols[0] = 2;

    while (i < len)
    {
        int start;
        int word_len;
        int line_start;
        int j;

        /* Пробел */
        if (buffer[i] == ' ')
        {
            screen_rows[i] = row;
            screen_cols[i] = col;

            if (col < MAX_LINE)
            {
                if (draw)
                    putchar(' ');

                col++;
            }

            i++;

            screen_rows[i] = row;
            screen_cols[i] = col;

            continue;
        }

        /* Находим конец слова */
        start = i;

        while (i < len && buffer[i] != ' ')
            i++;

        word_len = i - start;

        line_start = (row == 0) ? 2 : 0;

        /*
         * Если слово не помещается в оставшуюся часть
         * строки, переносим его целиком на новую строку.
         */
        if (col > line_start && col + word_len > MAX_LINE)
        {
            if (draw)
                putchar('\n');

            row++;
            col = 0;
        }

        /* Выводим слово с переносом длинных слов */
        for (j = 0; j < word_len; j++)
        {
            int pos = start + j;

            if (col == MAX_LINE)
            {
                if (draw)
                    putchar('\n');

                row++;
                col = 0;
            }

            screen_rows[pos] = row;
            screen_cols[pos] = col;

            if (draw)
                putchar(buffer[pos]);

            col++;

            screen_rows[pos + 1] = row;
            screen_cols[pos + 1] = col;
        }
    }

    screen_rows[len] = row;
    screen_cols[len] = col;
}

/*
 * Перемещаем терминальный курсор из одной строки
 * отображения в другую.
 */
void position_terminal_cursor(int from_row,
                              int target_row,
                              int target_col)
{
    if (from_row > target_row)
        printf("\033[%dA", from_row - target_row);
    else if (target_row > from_row)
        printf("\033[%dB", target_row - from_row);

    printf("\r");

    if (target_col > 0)
        printf("\033[%dC", target_col);

    cursor_screen_line = target_row;
}

/*
 * Полностью перерисовываем текст и возвращаем курсор
 * в позицию cursor_pos.
 */
void redraw(const char *buffer, int len)
{
    clear_previous_output();

    printf("> ");

    build_layout(buffer, len, 1);

    previous_lines = screen_rows[len] + 1;

    position_terminal_cursor(
        screen_rows[len],
        screen_rows[cursor_pos],
        screen_cols[cursor_pos]);

    fflush(stdout);
}

/*
 * Ищем позицию на указанной визуальной строке,
 * ближайшую к нужной колонке.
 */
int find_position_on_line(int len, int target_row, int target_col)
{
    int i;
    int best_pos = -1;
    int best_distance = BUFFER_SIZE + 1;

    for (i = 0; i <= len; i++)
    {
        int distance;

        if (screen_rows[i] != target_row)
            continue;

        distance = screen_cols[i] - target_col;

        if (distance < 0)
            distance = -distance;

        if (best_pos == -1 || distance < best_distance)
        {
            best_pos = i;
            best_distance = distance;
        }
    }

    return best_pos;
}

/*
 * Перемещение по визуальным строкам.
 *
 * direction = -1: вверх
 * direction =  1: вниз
 */
void move_vertical(char *buffer, int len, int direction)
{
    int current_row;
    int target_row;
    int target_pos;

    current_row = screen_rows[cursor_pos];
    target_row = current_row + direction;

    if (target_row < 0 || target_row >= previous_lines)
    {
        bell();
        return;
    }

    /*
     * При первом вертикальном перемещении запоминаем
     * желаемую колонку. Она сохраняется при движении
     * через короткие строки.
     */
    if (desired_column == -1)
        desired_column = screen_cols[cursor_pos];

    target_pos = find_position_on_line(
        len,
        target_row,
        desired_column);

    if (target_pos == -1)
    {
        bell();
        return;
    }

    cursor_pos = target_pos;

    redraw(buffer, len);
}

/*
 * Удаляем слово слева от курсора вместе
 * с пробелами непосредственно перед ним.
 */
int delete_word_before_cursor(char *buffer,
                              int len,
                              int *position)
{
    int start = *position;
    int count;
    int i;

    while (start > 0 && buffer[start - 1] == ' ')
        start--;

    while (start > 0 && buffer[start - 1] != ' ')
        start--;

    count = *position - start;

    for (i = start; i < len - count; i++)
        buffer[i] = buffer[i + count];

    len -= count;
    *position = start;
    buffer[len] = '\0';

    return len;
}

int main(void)
{
    char buffer[BUFFER_SIZE];
    int len = 0;

    struct termios original;

    if (tcgetattr(STDIN_FILENO, &original) == -1)
    {
        perror("tcgetattr");
        return 1;
    }

    buffer[0] = '\0';

    enable_raw_mode();

    /* Начальная отрисовка нужна для расчёта координат */
    redraw(buffer, len);

    while (1)
    {
        char c;

        if (read(STDIN_FILENO, &c, 1) != 1)
            break;

        /*
         * Стрелки терминала:
         * ESC [ A — вверх
         * ESC [ B — вниз
         * ESC [ C — вправо
         * ESC [ D — влево
         */
        if (c == '\033')
        {
            char sequence[2];
            int n = (int)read(STDIN_FILENO, sequence, 2);

            if (n == 2 && sequence[0] == '[')
            {
                switch (sequence[1])
                {
                case 'A':
                    move_vertical(buffer, len, -1);
                    break;

                case 'B':
                    move_vertical(buffer, len, 1);
                    break;

                case 'C':
                    if (cursor_pos < len)
                    {
                        cursor_pos++;
                        desired_column = -1;
                        redraw(buffer, len);
                    }
                    else
                    {
                        bell();
                    }
                    break;

                case 'D':
                    if (cursor_pos > 0)
                    {
                        cursor_pos--;
                        desired_column = -1;
                        redraw(buffer, len);
                    }
                    else
                    {
                        bell();
                    }
                    break;

                default:
                    bell();
                    break;
                }
            }
            else
            {
                bell();
            }

            continue;
        }

        /*
         * Ctrl+D.
         * Выход разрешён только при пустом буфере.
         */
        if (c == 4)
        {
            if (len == 0)
            {
                printf("\nExiting...\n");
                break;
            }

            bell();
            continue;
        }

        /*
         * Backspace — удалить символ слева от курсора.
         */
        if (c == original.c_cc[VERASE])
        {
            if (cursor_pos > 0)
            {
                int i;

                for (i = cursor_pos - 1; i < len - 1; i++)
                    buffer[i] = buffer[i + 1];

                len--;
                cursor_pos--;

                buffer[len] = '\0';
                desired_column = -1;

                redraw(buffer, len);
            }
            else
            {
                bell();
            }

            continue;
        }

        /*
         * Ctrl+U — очистить весь текущий буфер.
         */
        if (c == original.c_cc[VKILL])
        {
            if (len > 0)
            {
                len = 0;
                cursor_pos = 0;
                buffer[0] = '\0';

                desired_column = -1;

                redraw(buffer, len);
            }

            continue;
        }

        /*
         * Ctrl+W — удалить слово слева от курсора.
         */
        if (c == 23)
        {
            if (cursor_pos > 0)
            {
                len = delete_word_before_cursor(
                    buffer,
                    len,
                    &cursor_pos);

                desired_column = -1;

                redraw(buffer, len);
            }
            else
            {
                bell();
            }

            continue;
        }

        /*
         * Ctrl+P — предыдущая визуальная строка.
         */
        if (c == 16)
        {
            move_vertical(buffer, len, -1);
            continue;
        }

        /*
         * Ctrl+N — следующая визуальная строка.
         */
        if (c == 14)
        {
            move_vertical(buffer, len, 1);
            continue;
        }

        /*
         * Enter — завершить текущую строку ввода
         * и начать новую.
         */
        if (c == '\n' || c == '\r')
        {
            /*
             * Перемещаем курсор в конец текста,
             * чтобы новая строка начиналась после него.
             */
            position_terminal_cursor(
                cursor_screen_line,
                screen_rows[len],
                screen_cols[len]);

            putchar('\n');

            len = 0;
            cursor_pos = 0;
            buffer[0] = '\0';

            previous_lines = 1;
            cursor_screen_line = 0;
            desired_column = -1;

            redraw(buffer, len);

            continue;
        }

        /*
         * Вставка печатаемого символа в позицию курсора.
         */
        if (isprint((unsigned char)c))
        {
            if (len < BUFFER_SIZE - 1)
            {
                int i;

                for (i = len; i > cursor_pos; i--)
                    buffer[i] = buffer[i - 1];

                buffer[cursor_pos] = c;

                len++;
                cursor_pos++;

                buffer[len] = '\0';
                desired_column = -1;

                redraw(buffer, len);
            }
            else
            {
                bell();
            }

            continue;
        }

        /* Любой другой непечатаемый символ */
        bell();
    }

    return 0;
}
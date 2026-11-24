#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#define MAX_LINE_LEN 40
#define MAX_LINES 1024

static struct termios orig_termios;
static char erase_char;
static char kill_char;

// Буфер всех строк и позиция курсора
static char lines[MAX_LINES][MAX_LINE_LEN + 1];
static int line_lens[MAX_LINES];
static int num_lines = 1;
static int cur_row = 0;
static int cur_col = 0;
static int cursor_mode = 0; // 0 = обычный ввод, 1 = включён режим стрелок (Ctrl+P)

// Функция восстановления обычного режима терминала при выходе
void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Функция включения "сырого" режима терминала (Raw Mode)
void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    erase_char = orig_termios.c_cc[VERASE];
    kill_char = orig_termios.c_cc[VKILL];

    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

// Чтение байта с таймаутом (чтобы отличать одиночный Esc от стрелок \x1b[A)
int read_byte_timeout(char *out, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        return read(STDIN_FILENO, out, 1) == 1;
    }
    return 0;
}

// Перемещение физического курсора терминала из (from_row, from_col) в (to_row, to_col)
void move_terminal_cursor(int from_row, int from_col, int to_row, int to_col) {
    char buf[32];
    int n;

    if (to_row < from_row) {
        n = snprintf(buf, sizeof(buf), "\x1b[%dA", from_row - to_row);
        write(STDOUT_FILENO, buf, n);
    } else if (to_row > from_row) {
        n = snprintf(buf, sizeof(buf), "\x1b[%dB", to_row - from_row);
        write(STDOUT_FILENO, buf, n);
    }

    write(STDOUT_FILENO, "\r", 1);
    if (to_col > 0) {
        n = snprintf(buf, sizeof(buf), "\x1b[%dC", to_col);
        write(STDOUT_FILENO, buf, n);
    }
    (void)from_col;
}

// Перерисовка экрана начиная со строки start_row (используется при вставке новой строки выше конца)
void redraw_lines_from(int start_row, int from_row, int from_col, int target_row, int target_col) {
    move_terminal_cursor(from_row, from_col, start_row, 0);
    write(STDOUT_FILENO, "\x1b[J", 3); // Очистить от курсора до конца экрана

    for (int r = start_row; r < num_lines; r++) {
        if (line_lens[r] > 0) {
            write(STDOUT_FILENO, lines[r], line_lens[r]);
        }
        if (r < num_lines - 1) {
            write(STDOUT_FILENO, "\r\n", 2);
        }
    }

    move_terminal_cursor(num_lines - 1, line_lens[num_lines - 1], target_row, target_col);
}

// Удаление одного символа слева от курсора (работает в любом месте строки)
void erase_char_at_cursor(void) {
    if (cur_col <= 0) {
        return;
    }

    int old_len = line_lens[cur_row];
    int tail_len = old_len - cur_col;

    if (tail_len > 0) {
        memmove(&lines[cur_row][cur_col - 1], &lines[cur_row][cur_col], tail_len);
    }

    line_lens[cur_row]--;
    cur_col--;

    if (tail_len == 0) {
        write(STDOUT_FILENO, "\b \b", 3);
    } else {
        write(STDOUT_FILENO, "\b", 1);
        write(STDOUT_FILENO, &lines[cur_row][cur_col], tail_len);
        write(STDOUT_FILENO, " ", 1);

        char buf[32];
        int n = snprintf(buf, sizeof(buf), "\x1b[%dD", tail_len + 1);
        write(STDOUT_FILENO, buf, n);
    }
}

// Вставка символа на текущую позицию (без переноса строки)
void insert_char_in_line(char c) {
    int old_len = line_lens[cur_row];
    int tail_len = old_len - cur_col;

    if (tail_len == 0) {
        lines[cur_row][cur_col++] = c;
        line_lens[cur_row]++;
        write(STDOUT_FILENO, &c, 1);
    } else {
        memmove(&lines[cur_row][cur_col + 1], &lines[cur_row][cur_col], tail_len);
        lines[cur_row][cur_col] = c;
        line_lens[cur_row]++;
        cur_col++;

        write(STDOUT_FILENO, &lines[cur_row][cur_col - 1], tail_len + 1);

        char buf[32];
        int n = snprintf(buf, sizeof(buf), "\x1b[%dD", tail_len);
        write(STDOUT_FILENO, buf, n);
    }
}

int main(void) {
    enable_raw_mode();

    char c;

    while (read(STDIN_FILENO, &c, 1) == 1) {
        // 0. CTRL-P (код 16) — включение/выключение режима перемещения стрелками по строкам
        if (c == 16) {
            cursor_mode = !cursor_mode;
            if (!cursor_mode) {
                // При выключении режима курсора возвращаем его в конец текста
                int old_row = cur_row;
                int old_col = cur_col;
                cur_row = num_lines - 1;
                cur_col = line_lens[cur_row];
                if (old_row != cur_row || old_col != cur_col) {
                    move_terminal_cursor(old_row, old_col, cur_row, cur_col);
                }
            }
            continue;
        }

        // 0.5. Обработка ESC и стрелок клавиатуры (\x1b[A, B, C, D или \x1bOA, B, C, D)
        if (c == 27) {
            char seq[2];
            if (read_byte_timeout(&seq[0], 50) && (seq[0] == '[' || seq[0] == 'O') &&
                read_byte_timeout(&seq[1], 50)) {

                if (!cursor_mode) {
                    write(STDOUT_FILENO, "\a", 1);
                    continue;
                }

                int old_row = cur_row;
                int old_col = cur_col;

                if (seq[1] == 'A') { // Стрелка ВВЕРХ
                    if (cur_row > 0) {
                        cur_row--;
                        if (cur_col > line_lens[cur_row]) {
                            cur_col = line_lens[cur_row];
                        }
                        move_terminal_cursor(old_row, old_col, cur_row, cur_col);
                    } else {
                        write(STDOUT_FILENO, "\a", 1);
                    }
                } else if (seq[1] == 'B') { // Стрелка ВНИЗ
                    if (cur_row + 1 < num_lines) {
                        cur_row++;
                        if (cur_col > line_lens[cur_row]) {
                            cur_col = line_lens[cur_row];
                        }
                        move_terminal_cursor(old_row, old_col, cur_row, cur_col);
                    } else {
                        write(STDOUT_FILENO, "\a", 1);
                    }
                } else if (seq[1] == 'C') { // Стрелка ВПРАВО
                    if (cur_col < line_lens[cur_row]) {
                        cur_col++;
                        move_terminal_cursor(old_row, old_col, cur_row, cur_col);
                    } else if (cur_row + 1 < num_lines) {
                        cur_row++;
                        cur_col = 0;
                        move_terminal_cursor(old_row, old_col, cur_row, cur_col);
                    } else {
                        write(STDOUT_FILENO, "\a", 1);
                    }
                } else if (seq[1] == 'D') { // Стрелка ВЛЕВО
                    if (cur_col > 0) {
                        cur_col--;
                        move_terminal_cursor(old_row, old_col, cur_row, cur_col);
                    } else if (cur_row > 0) {
                        cur_row--;
                        cur_col = line_lens[cur_row];
                        move_terminal_cursor(old_row, old_col, cur_row, cur_col);
                    } else {
                        write(STDOUT_FILENO, "\a", 1);
                    }
                } else {
                    write(STDOUT_FILENO, "\a", 1);
                }
                continue;
            } else {
                write(STDOUT_FILENO, "\a", 1);
                continue;
            }
        }

        // 1. Выход по CTRL-D (код 4) только если курсор в начале строки
        if (c == 4) {
            if (cur_col == 0) {
                move_terminal_cursor(cur_row, cur_col, num_lines - 1, line_lens[num_lines - 1]);
                write(STDOUT_FILENO, "\r\nExiting...\r\n", 14);
                break;
            } else {
                write(STDOUT_FILENO, "\a", 1);
                continue;
            }
        }

        // 2. Обработка ERASE (Backspace / Del / системный символ)
        if (c == erase_char || c == 127 || c == '\b') {
            erase_char_at_cursor();
            continue;
        }

        // 3. Обработка KILL (Ctrl+U / системный символ) — стереть всё до курсора
        if (c == kill_char || c == 21) {
            while (cur_col > 0) {
                erase_char_at_cursor();
            }
            continue;
        }

        // 4. Обработка CTRL-W (код 23) — стереть последнее слово и пробелы перед курсором
        if (c == 23) {
            while (cur_col > 0 && lines[cur_row][cur_col - 1] == ' ') {
                erase_char_at_cursor();
            }
            while (cur_col > 0 && lines[cur_row][cur_col - 1] != ' ') {
                erase_char_at_cursor();
            }
            continue;
        }

        // 5. Обработка Enter (\n или \r) — перенос на новую строку
        if (c == '\n' || c == '\r') {
            if (num_lines >= MAX_LINES) {
                write(STDOUT_FILENO, "\a", 1);
                continue;
            }

            if (cur_row == num_lines - 1 && cur_col == line_lens[cur_row]) {
                cur_row++;
                cur_col = 0;
                line_lens[cur_row] = 0;
                num_lines++;
                write(STDOUT_FILENO, "\r\n", 2);
            } else {
                for (int r = num_lines; r > cur_row + 1; r--) {
                    memcpy(lines[r], lines[r - 1], line_lens[r - 1]);
                    line_lens[r] = line_lens[r - 1];
                }
                num_lines++;

                int tail_len = line_lens[cur_row] - cur_col;
                if (tail_len > 0) {
                    memcpy(lines[cur_row + 1], &lines[cur_row][cur_col], tail_len);
                }
                line_lens[cur_row + 1] = tail_len;
                line_lens[cur_row] = cur_col;

                int old_row = cur_row, old_col = cur_col;
                cur_row++;
                cur_col = 0;
                redraw_lines_from(old_row, old_row, old_col, cur_row, cur_col);
            }
            continue;
        }

        // 6. Печатаемые символы (включая пробел)
        if (isprint((unsigned char)c)) {
            if (line_lens[cur_row] < MAX_LINE_LEN) {
                insert_char_in_line(c);
            } else {
                if (cur_col < MAX_LINE_LEN) {
                    write(STDOUT_FILENO, "\a", 1);
                    continue;
                }
                if (num_lines >= MAX_LINES) {
                    write(STDOUT_FILENO, "\a", 1);
                    continue;
                }

                for (int r = num_lines; r > cur_row + 1; r--) {
                    memcpy(lines[r], lines[r - 1], line_lens[r - 1]);
                    line_lens[r] = line_lens[r - 1];
                }
                num_lines++;

                int is_last_line = (cur_row + 1 == num_lines - 1);

                if (c == ' ') {
                    line_lens[cur_row + 1] = 0;
                    if (is_last_line) {
                        cur_row++;
                        cur_col = 0;
                        write(STDOUT_FILENO, "\r\n", 2);
                    } else {
                        int old_row = cur_row;
                        cur_row++;
                        cur_col = 0;
                        redraw_lines_from(old_row, old_row, MAX_LINE_LEN, cur_row, cur_col);
                    }
                } else {
                    int word_start = MAX_LINE_LEN;
                    while (word_start > 0 && lines[cur_row][word_start - 1] != ' ') {
                        word_start--;
                    }

                    if (word_start == 0) {
                        lines[cur_row + 1][0] = c;
                        line_lens[cur_row + 1] = 1;
                        if (is_last_line) {
                            cur_row++;
                            cur_col = 1;
                            write(STDOUT_FILENO, "\r\n", 2);
                            write(STDOUT_FILENO, &c, 1);
                        } else {
                            int old_row = cur_row;
                            cur_row++;
                            cur_col = 1;
                            redraw_lines_from(old_row, old_row, MAX_LINE_LEN, cur_row, cur_col);
                        }
                    } else {
                        int part_len = MAX_LINE_LEN - word_start;
                        memcpy(lines[cur_row + 1], &lines[cur_row][word_start], part_len);
                        lines[cur_row + 1][part_len] = c;
                        line_lens[cur_row + 1] = part_len + 1;
                        line_lens[cur_row] = word_start;

                        if (is_last_line) {
                            for (int i = 0; i < part_len; i++) {
                                write(STDOUT_FILENO, "\b \b", 3);
                            }
                            write(STDOUT_FILENO, "\r\n", 2);
                            cur_row++;
                            cur_col = part_len + 1;
                            write(STDOUT_FILENO, lines[cur_row], cur_col);
                        } else {
                            int old_row = cur_row;
                            cur_row++;
                            cur_col = part_len + 1;
                            redraw_lines_from(old_row, old_row, MAX_LINE_LEN, cur_row, cur_col);
                        }
                    }
                }
            }
            continue;
        }

        // 7. Все остальные непечатаемые символы издают звуковой сигнал CTRL-G (\a)
        write(STDOUT_FILENO, "\a", 1);
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

typedef struct Node {
    char *text;
    struct Node *next;
} Node;

/*
 * Each new line goes to the front of the list, so the list ends up in
 * reverse input order and a plain walk from head prints it backwards.
 */
static int push_front(Node **head, const char *text)
{
    Node *node = malloc(sizeof(*node));
    size_t length = strlen(text);

    if (node == NULL) {
        perror("malloc node");
        return -1;
    }
    node->text = malloc(length + 1);
    if (node->text == NULL) {
        perror("malloc text");
        free(node);
        return -1;
    }
    memcpy(node->text, text, length + 1);
    node->next = *head;
    *head = node;
    return 0;
}

static void free_list(Node *head)
{
    while (head != NULL) {
        Node *next = head->next;
        free(head->text);
        free(head);
        head = next;
    }
}

int main(void)
{
    Node *head = NULL;
    char buffer[BUFFER_SIZE];

    puts("Enter lines; a line starting with '.' ends input:");
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t length = strlen(buffer);
        int has_newline = length > 0 && buffer[length - 1] == '\n';

        if (has_newline)
            buffer[--length] = '\0';
        else if (!feof(stdin)) {
            int character;
            while ((character = getchar()) != '\n' && character != EOF) {}
            fprintf(stderr, "Input line exceeds %d characters; skipped\n", BUFFER_SIZE - 1);
            continue;
        }
        if (buffer[0] == '.')
            break;
        /* Empty lines are stored too: length 0 still gets a 1-byte "\0" string. */
        if (push_front(&head, buffer) == -1) {
            free_list(head);
            return EXIT_FAILURE;
        }
    }
    if (ferror(stdin)) {
        perror("stdin");
        free_list(head);
        return EXIT_FAILURE;
    }

    puts("Stored lines (reverse order):");
    for (Node *node = head; node != NULL; node = node->next)
        puts(node->text);
    free_list(head);
    return EXIT_SUCCESS;
}

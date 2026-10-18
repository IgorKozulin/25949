#include <stdio.h>        // printf, fgets, stdin
#include <stdlib.h>       // malloc, free, exit
#include <string.h>       // strlen, strcpy

struct Node {
    char *data;        // Указатель на динамическую строку
    struct Node *next; // Указатель на следующий узел
};

void append (struct Node **head, char *str) {
    struct Node *node = malloc(sizeof(struct Node));
    if (node == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    char *a = malloc(strlen(str) + 1);
    if (a == NULL) {
        perror("malloc");
        free(node);
        exit(EXIT_FAILURE);
    }

    strcpy(a, str);

    node->data = a;
    node->next = NULL;

    if (*head == NULL) {
        *head = node;
    }
    else {
        struct Node *cur = *head;
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = node;
    }
}

void free_list(struct Node **head) {
    struct Node *node = *head;
    while (node != NULL) {
        struct Node *next = node->next;
        free(node->data);
        free(node);
        node = next;
    }
    *head = NULL; 
}

int main(void) {
    struct Node *head = NULL;
    char buffer[1024];

    while(1) {
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        if (strlen(buffer) > 0 && buffer[strlen(buffer)-1] == '\n') {
            buffer[strlen(buffer)-1] = '\0';
        }

        if (buffer[0] == '.') {
            break;
        }
        
        if (buffer[0]!= '\0') {
            append(&head, buffer);
        }
    }

    struct Node *cur = head;
    
    while (cur != NULL) {
        printf("%s\n", cur->data);
        cur = cur->next;
    }

    free_list(&head);

    return 0;
}
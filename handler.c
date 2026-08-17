#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "handler.h"
#include "job.h"

void cd_handler(int argc, char *argv[]){
    if (argc != 2){
        fprintf(stderr, "cd has to have 2 arguments\n");
        return;
    }
    int ret = chdir(argv[1]);
    if (ret == -1){
        fprintf(stderr, "cd execution failed\n");
    }
    return;
}
void pwd_handler(int argc){
    if (argc != 1){
        fprintf(stderr, "pwd has to have 1 argument\n");
        return;
    }
    char buf[100];
    char *path = getcwd(buf, sizeof(buf));
    if (path == NULL){
        fprintf(stderr, "pwd execution failed\n");
    }
    else printf("%s\n", path);
    return;
}
void jobs_handler(int argc, struct Job *dummy){
    if (argc != 1){
        fprintf(stderr, "jobs has to have 1 argument\n");
        return;
    }
    struct Job *ptr;
    ptr = dummy;
    int j = 1;
    while (ptr->next != NULL){
        ptr = ptr->next;
        printf("[%02d] %d %s\n", j, ptr->pid, ptr->command);
        j++;
    }
    if (j == 1){
        printf("Background process not exist\n");
        return;
    }
    return;
}
void free_heap(struct Job *dummy){
    struct Job *ptr;
    struct Job *prev;
    ptr = dummy;
    while (ptr->next != NULL){
        prev = ptr;
        ptr = ptr->next;
        free(prev);
    }
    free(ptr);
}
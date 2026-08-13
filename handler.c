#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "handler.h"

void cd_handler(int argc, char *argv[]){
    if (argc != 2){
        printf("cd has to have 2 arguments\n");
        return;
    }
    int ret = chdir(argv[1]);
    if (ret == -1){
        printf("cd execution failed\n");
    }
    return;
}
void pwd_handler(int argc){
    if (argc != 1){
        printf("pwd has to have 1 argument\n");
        return;
    }
    char buf[100];
    char *path = getcwd(buf, sizeof(buf));
    if (path == NULL){
        printf("pwd execution failed\n");
    }
    else printf("%s\n", path);
    return;
}
void exit_handler(int argc){
    if (argc != 1){
        printf("exit has to have 1 argument\n");
        return;
    }
    exit(0);
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "handler.h"


int main(void){
    char command[100];
    char *argv[10];

    while (1){
        printf("$ ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        char *token = strtok(command, " ");
        int i;
        for (i = 0; token != NULL; i++){
            argv[i] = token;
            token = strtok(NULL, " ");
        }
        argv[i] = NULL; //NULL 값으로 배열이 여기서 끝난다는 것을 표시
        
        if (i == 0) continue;

        if (strcmp(argv[0], "cd") == 0){
            cd_handler(i, argv);
            continue;
        }
        if (strcmp(argv[0], "pwd") == 0){
            pwd_handler(i);
            continue;
        }
        if (strcmp(argv[0], "exit") == 0){
            exit_handler(i);
            continue;
        }

        pid_t pid = fork();
        if (pid == -1){
            printf("Fork failed\n");
            continue;
        }
        if (pid == 0){
            int ret = execvp(argv[0], argv);
            printf("Command execution failed\n");
            exit(1);
        }
        else {
            int status;
            waitpid(pid, &status, 0); //자식 프로세스가 exit되며 부모 프로세스가 SIGCHLD 시그널을 받을때까지 blocked
        }
    }
}
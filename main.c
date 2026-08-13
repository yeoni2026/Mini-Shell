#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "handler.h"
#include <fcntl.h>

#define COMMAND_SIZE 100
#define ARGV_SIZE 10

void restore_stdin(int fd, int save){
    dup2(save, 0);
    close(fd);    
}
void restore_stdout(int fd, int save){
    dup2(save, 1);
    close(fd);    
}

int main(void){
    char command[COMMAND_SIZE];
    char *argv[ARGV_SIZE];

    while (1){
        printf("$ ");

        if (fgets(command, COMMAND_SIZE, stdin) == NULL) { //NULL값 확인하는 이유: stdin에서 EOF플래그가 올라갈 시 앞으로의 입력이 모두 NULL처리 되면서 무한루프될 수 있으므로 방지
            printf("exit\n");
            exit(0);
        }
        command[strcspn(command, "\n")] = '\0';

        char *token = strtok(command, " ");
        int i;
        for (i = 0; token != NULL && i < ARGV_SIZE - 1; i++){
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
        
        //IO 리다이렉션 
        int error_flag = 0;
        int fileout_flag = 0;
        int filein_flag = 0;
        int fd_out, fd_in, save_in, save_out;
        char message[100]; //바로바로 printf 출력을 하지 않고 message에 문구를 담아 이후 error_flag 처리에서 출력하는 이유: for 반복문 내에서 리다이렉션이 즉각적으로 이루어지기에 message가 표준출력(stdout)으로 출력되지 않을 위험이 있기 때문

        for (int j = 0; j < i; j++){
            if (strcmp(argv[j], ">") == 0){
                if (j + 1 == i){
                    strcpy(message, "Missing output file");
                    error_flag = 1;
                    break;
                }
                if (fileout_flag){
                    strcpy(message, "too many redirections");
                    error_flag = 1;
                    break;
                }

                fd_out = open(argv[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                save_out = dup(1); //표준 출력 어딘가에 저장해놓기 > 그래야 복구 가능
                dup2(fd_out, 1);
                fileout_flag = 1;
                argv[j] = NULL;
            }
            else if (strcmp(argv[j], "<") == 0){
                if (j + 1 == i){
                    strcpy(message, "Missing input file");
                    error_flag = 1;
                    break;
                }
                if (filein_flag){
                    strcpy(message, "too many redirections");
                    error_flag = 1;
                    break;
                }

                fd_in = open(argv[j + 1], O_RDONLY);
                if (fd_in == -1){
                    strcpy(message, "File not exist");
                    error_flag = 1;
                    break;
                }
                save_in = dup(0); //표준 입력 어딘가에 저장해놓기 > 그래야 복구 가능
                dup2(fd_in, 0);
                filein_flag = 1;
                argv[j] = NULL;
            }
        }
        if (error_flag) {
            if (fileout_flag) restore_stdout(fd_out, save_out);
            if (filein_flag) restore_stdin(fd_in, save_in);
            printf("%s\n", message);
            continue;
        }

        pid_t pid = fork();
        if (pid == -1){
            printf("Fork failed\n");
            if (fileout_flag) restore_stdout(fd_out, save_out);
            if (filein_flag) restore_stdin(fd_in, save_in);
            continue;
        }
        if (pid == 0){
            execvp(argv[0], argv);
            printf("Command execution failed\n");
            exit(1);
        }
        else {
            int status;
            waitpid(pid, &status, 0); //자식 프로세스가 exit되며 부모 프로세스가 SIGCHLD 시그널을 받을때까지 blocked
            if (fileout_flag) restore_stdout(fd_out, save_out);
            if (filein_flag) restore_stdin(fd_in, save_in);
        }
    }
}
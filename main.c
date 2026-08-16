#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "handler.h"
#include <fcntl.h>
#include "job.h"


#define COMMAND_SIZE 100
#define ARGV_SIZE 10
#define IDX(i, j) ((i) * 2 + (j)) //i, j에 괄호를 쳐야하는 이유!: 예를들어 i에 command_num - 1이 들어가면, #define은 순수 텍스트 치환이기 때문에 command_num - 1 * 2로 변환함;; 

void redirection(int argc, char *argv[]);

int main(void){
    char command[COMMAND_SIZE];
    char *argv[ARGV_SIZE];
    pid_t pid[ARGV_SIZE];
    
    struct Job *dummy = malloc(sizeof(struct Job));
    if (dummy == NULL){
        printf("malloc failed\n");
        exit(1);
    }
    struct Job *curr = dummy;

    while (1){
        int pid_count = 0;
        int status;

        while(1){ //백그라운드 프로세스 Zombie 처리
            pid_t back_pid = waitpid(-1, &status, WNOHANG);
            if (back_pid > 0) {
                struct Job *ptr;
                struct Job *prev;
                ptr = dummy;
                while (ptr->next != NULL){
                    prev = ptr;
                    ptr = ptr->next;
                    if (ptr->pid == back_pid){
                        prev->next = ptr->next; //연결리스트 중간 인덱스값 제거: prev 구조체의 next 포인터 주소를 ptr의 next 포인터가 가리키는 구조체로 연결
                        if (ptr == curr){
                            curr = prev;
                        }
                        free(ptr);
                        break;
                    }
                }
            }
            else break;
        }

        printf("$ ");

        if (fgets(command, COMMAND_SIZE, stdin) == NULL) { //NULL값 확인하는 이유: stdin에서 EOF플래그가 올라갈 시 앞으로의 입력이 모두 NULL처리 되면서 무한루프될 수 있으므로 방지
            printf("exit\n");
            free(dummy);
            exit(0);
        }

        char *newline = strchr(command, '\n');

        if (newline != NULL) *newline = '\0';
        else {
            int c;
            while ((c = getchar()) != EOF && c != '\n');
        }

        char *token = strtok(command, " ");
        int i;
        for (i = 0; token != NULL && i < ARGV_SIZE - 1; i++){
            argv[i] = token;
            token = strtok(NULL, " ");
        }
        argv[i] = NULL; //NULL 값으로 배열이 여기서 끝난다는 것을 표시
        
        if (i == 0) continue;

        if (strcmp(argv[0], "exit") == 0){
            if (i != 1){
                printf("exit has to have 1 argument\n");
                continue;
            }
            exit(0);    
        }
        if (strcmp(argv[0], "cd") == 0){
            cd_handler(i, argv);
            continue;
        }
        if (strcmp(argv[0], "pwd") == 0){
            pwd_handler(i);
            continue;
        }
        
        if (strcmp(argv[0], "jobs") == 0){
            jobs_handler(i, dummy);
            continue;
        }

        //Pipe
        int error_flag = 0;
        int command_num = 0;
        int start_point = 0;
        int fd[64];

        for (int j = 0; j < i; j++){
            if (strcmp(argv[j], "|") == 0){
                if (j == 0 || j == i - 1 || argv[j - 1] == NULL){
                    printf("Missing command\n");
                    error_flag = 1;
                    break;
                }
                argv[j] = NULL;
                pipe(fd + 2 * command_num);
                
                pid[pid_count] = fork();

                if (pid[pid_count] == -1){
                    printf("Fork failed\n");
                    error_flag = 1;
                    break;
                }
                if (pid[pid_count] == 0){
                    if (command_num != 0) {
                        dup2(fd[IDX(command_num - 1, 0)], 0);
                        close(fd[IDX(command_num - 1, 0)]);
                    }
                    int save_out = dup(1);
                    dup2(fd[IDX(command_num, 1)], 1);
                    close(fd[IDX(command_num, 1)]);
                    close(fd[IDX(command_num, 0)]);
                    
                    execvp(argv[start_point], argv + start_point);
                    dup2(save_out, 1);
                    printf("Command execution failed\n");
                    exit(1);
                }
                if (command_num != 0) close(fd[IDX(command_num - 1, 0)]);
                close(fd[IDX(command_num, 1)]);
                start_point = j + 1;
                command_num++;
                pid_count++;
            }
        }
        if (error_flag){
            for (int j = 0; j < pid_count; j++){
                waitpid(pid[j], &status, 0);
            }
            continue;
        }

        if (strcmp(argv[i - 1], "&") == 0 && command_num){
            printf("Pipes and background execution cannot be used together\n");
            continue;
        }

        pid[pid_count] = fork();

        if (pid[pid_count] == -1){
            printf("Fork failed\n");
            continue;
        }
        if (pid[pid_count] == 0){
            if (strcmp(argv[i - 1], "&") == 0){
                argv[i - 1] = NULL;
            }
            else if (command_num) {
                dup2(fd[IDX(command_num - 1, 0)], 0);
                close(fd[IDX(command_num - 1, 0)]);
            }
            else redirection(i, argv + start_point);
            execvp(argv[start_point], argv + start_point);
            printf("Command execution failed\n");
            exit(1);
        }
        else {
            int background_flag = 0;
            if (strcmp(argv[i - 1], "&") == 0){
                curr->next = malloc(sizeof(struct Job));
                curr = curr->next;
                if (curr == NULL){
                    printf("malloc failed\n");
                    continue;
                }

                curr->pid = pid[pid_count];
                strcpy(curr->command, argv[0]);
                curr->next = NULL;

                background_flag = 1;
            }

            if (command_num) close(fd[IDX(command_num - 1, 0)]);
            if (!background_flag){
                for (int j = 0; j <= pid_count; j++){
                    waitpid(pid[j], &status, 0);
                }
            }
        }
    }
}

void redirection(int argc, char *argv[]){
    //IO 리다이렉션 
    int error_flag = 0;
    int fileout_flag = 0;
    int filein_flag = 0;
    int fd_out, fd_in, save_out;
    char message[100]; //바로바로 printf 출력을 하지 않고 message에 문구를 담아 이후 error_flag 처리에서 출력하는 이유: for 반복문 내에서 리다이렉션이 즉각적으로 이루어지기에 message가 표준출력(stdout)으로 출력되지 않을 위험이 있기 때문

    for (int j = 0; j < argc; j++){
        if (strcmp(argv[j], ">") == 0){
            if (j + 1 == argc){
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
            save_out = dup(1);
            dup2(fd_out, 1);
            close(fd_out);
            fileout_flag = 1;
            argv[j] = NULL;
        }
        else if (strcmp(argv[j], "<") == 0){
            if (j + 1 == argc){
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
            dup2(fd_in, 0);
            close(fd_in);
            filein_flag = 1;
            argv[j] = NULL;
        }
    }
    if (error_flag) {
        dup2(save_out, 1);
        printf("%s\n", message);
        exit(1);
    }
}
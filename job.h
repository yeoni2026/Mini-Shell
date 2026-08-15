#ifndef JOB_H
#define JOB_H

struct Job { //연결리스트!
    pid_t pid;
    char command[100];
    struct Job *next;
};

#endif
#ifndef HANDLER_H
#define HANDLER_H

#include "job.h"

void cd_handler(int argc, char *argv[]);
void pwd_handler(int argc);
void jobs_handler(int argc, struct Job *dummy);

#endif
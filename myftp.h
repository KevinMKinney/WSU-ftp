#ifndef MYFTP_H
#define MYFTP_H

// include c libraries
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include <linux/limits.h>
#include <fcntl.h>

// macros
#define PORT "49999"
#define BACKLOG 4
#define BUF_SIZE 256
#define INPUT_SIZE 5
#define NEW_FILE_PERMS (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) // chmod 755

#define DEBUG 0

#endif

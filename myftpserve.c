#include "myftp.h"

// send client errno message
void sendClientError(int fd, int errnum) {
	if (DEBUG) {
		fprintf(stderr, "%s\n", strerror(errnum));
	}

	if (write(fd, "E", 1) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		//exit(0);
	}
	//if (write(fd, strerror(errnum), sizeof(strerror(errnum))) < 0) {
	if (write(fd, strerror(errnum), (BUF_SIZE-2)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		//exit(0);
	}
	if (write(fd, "\n", 1) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		//exit(0);
	}
	return;
}

void runCD(int errfd, char* path) {
	if (path != NULL) {
		
		// check if user can access directory			
		if (access(path, R_OK) == -1) {
			sendClientError(errfd, errno);
		} else {
			chdir(path); // could error check this. But if this fails, there are bigger problems

			if (DEBUG) {
				printf("Changed directory to %s\n", path);
			}
		}

	}
	return;
}

void runLS(int sockfd) {
	int pid = fork();

	if (pid < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}

	if (!pid) {
		//child

		// setting up pipe
		int fd[2];

		if (pipe(fd) != 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(0);
		}

		// make the file descriptor sockfd the new stout
		close(1);
		dup(sockfd);
		close(sockfd);

		if (execlp("ls", "ls", "-l", NULL) == -1) { 
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		
		// end process
		close(sockfd);
		exit(0);
	}

	// parent waits for children to finish (no zombies/orphans)
	int status;
	waitpid(pid, &status, 0);
	// something for status here?

	return;
}

int main() {

	// setting up server socket
	int sockfd;
	struct sockaddr_in servaddr;

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(PORT));
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

	if (bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}

	listen(sockfd, BACKLOG);

	// init server vars
	int pid, connectfd;
	struct sockaddr_in clientaddr;
	unsigned int length = sizeof(struct sockaddr_in);

	// whenever a client connects to socket, fork a process to handle it
	while (1) {

		if ((connectfd = accept(sockfd, (struct sockaddr*) &clientaddr, &length)) < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(0);
		}

		pid = fork();

		if (pid < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(0);
		}

		if (!pid) {
			//child
			break;
		}
	}
	
	// more init vars 
	char input[2];
	memset(&input, '\0', sizeof(input));
	char *buf = malloc(BUF_SIZE * sizeof(char));

	int dataSockfd, fd, bRead; 
	int datafd = -1;
	struct sockaddr_in dataSockAddr;
	int dataSockLength = sizeof(dataSockAddr);
	char dataSockPort[16];
	struct stat s;

	while (1) {
		// get command
		if (read(connectfd, &input, 1) < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
		}

		if (input[0] != '\0') {

			if (DEBUG) {
				printf("Command %c\n", input[0]);
			}

			// create a data socket
			if (input[0] == 'D') {
				if (read(connectfd, &input, 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//sendClientError(connectfd, errno);
				}

				// setting up data socket
				
				memset(&dataSockAddr, 0, dataSockLength);
				dataSockAddr.sin_family = AF_INET;
				dataSockAddr.sin_port = htons(0);
				dataSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);


				if ((dataSockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
					sendClientError(connectfd, errno);
				}

				setsockopt(dataSockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

				if (bind(dataSockfd, (struct sockaddr*) &dataSockAddr, dataSockLength) < 0)  {
					sendClientError(connectfd, errno);
				}

				// find which port the socket was made on
				if (getsockname(dataSockfd, (struct sockaddr*) &dataSockAddr, (unsigned int*) &dataSockLength) < 0) {
					sendClientError(connectfd, errno);
				}

				sprintf(dataSockPort, "%d", ntohs(dataSockAddr.sin_port));
				
				if (DEBUG) {
					printf("Data Socket made on port %s\n", dataSockPort);
				}
				
				// send data to client
				if (write(connectfd, "A", 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}

				// dataSockLength = 16
				if (write(connectfd, &dataSockPort, dataSockLength) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}
				if (write(connectfd, "\n", 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}

				listen(dataSockfd, 1);

				if ((datafd = accept(dataSockfd, (struct sockaddr*) &dataSockAddr, (unsigned int*) &dataSockLength)) < 0) {
					sendClientError(connectfd, errno);
				}
				
				// ?
				if (write(connectfd, "A\n", 2) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}
			}

			if (input[0] == 'C') {
				// need to read pathname from client
				memset(buf, '\0', BUF_SIZE * sizeof(char));
				
				if ((bRead = read(connectfd, buf, BUF_SIZE)) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//sendClientError(connectfd, errno);
				}

				buf[bRead-1] = '\0';
				
				runCD(connectfd, buf);

				// send data to client
				if (write(connectfd, "A\n", 2) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}
			}

			if (input[0] == 'L') {
				//check if data connection has been made
				if (read(connectfd, &input, 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
				}

				if (datafd == -1) {
					if (write(connectfd, "EData connection not found\n", 28) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}
				} else {
					// assuming data socket has been made

					runLS(datafd);

					close(datafd);
					datafd = -1;
				}
			}

			if (input[0] == 'G') {
				//check if data connection has been made
				if (datafd == -1) {
					if (write(connectfd, "EData connection not found\n", 28) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}
				} else {

					// need to read pathname from client
					memset(buf, '\0', BUF_SIZE * sizeof(char));
					
					if ((bRead = read(connectfd, buf, BUF_SIZE)) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//sendClientError(connectfd, errno);
					}

					buf[bRead-1] = '\0';
					
					/*
					struct stat s;
					int fileInf = stat(inputPath, &s); */
					if (stat(buf, &s) < 0) {
						sendClientError(connectfd, errno);
					}
					
					if (S_ISREG(s.st_mode) && (s.st_mode & S_IRUSR)) {

						if ((fd = open(buf, O_RDONLY)) < 0) {
							sendClientError(connectfd, errno);
						}

						if (DEBUG) {
							printf("Reading %s's contents and writing to client\n", buf);
						}

						// read and write data from file to data socket
						while ((bRead = read(fd, buf, BUF_SIZE)) > 0) {
							//printf("%s\n", buf);
							if (write(datafd, buf, bRead) < 0) {
								fprintf(stderr, "%s\n", strerror(errno));
								//exit(0);
							}

							if (bRead < BUF_SIZE) {
								break;
							}
						}

						close(datafd);
						datafd = -1;
					} else {
						// send data to client
						if (write(connectfd, "EFile cannot be accessed\n", 26) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}
					}
				}
			}

			if (input[0] == 'P') {
				//check if data connection has been made
				if (datafd == -1) {
					if (write(connectfd, "EData connection not found\n", 28) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}
				} else {

					// need to read pathname from client
					memset(buf, '\0', BUF_SIZE * sizeof(char));
					
					if ((bRead = read(connectfd, buf, BUF_SIZE)) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//sendClientError(connectfd, errno);
					}

					buf[bRead-1] = '\0';
					
					if (access(buf, F_OK) == -1) {

						if ((fd = open(buf, O_CREAT | O_EXCL | O_WRONLY, NEW_FILE_PERMS)) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
						}

						if (DEBUG) {
							printf("Reading client and writing contents to %s\n", buf);
						}

						//memset(buf, '\0', BUF_SIZE * sizeof(char));

						while ((bRead = read(datafd, buf, BUF_SIZE)) > 0) {
							if (write(fd, buf, bRead) < 0) {
								fprintf(stderr, "%s\n", strerror(errno));
								//exit(0);
							}

							if (bRead < BUF_SIZE) {
								break;
							}
						}

						close(fd);
						close(datafd);
						datafd = -1;
					} else {
						// send data to client
						if (write(connectfd, "EFile already exists\n", 22) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}
					}
				}
			}

			// quit child server process 
			if (input[0] == 'Q') {

				if (read(connectfd, &input, 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
				}

				/*
				kill(0, SIGQUIT);
			
				int wstatus;
				if (waitpid(pid, &wstatus, 0) < 0) {
					// maybe do something with wstatus
					fprintf(stderr, "%s\n", strerror(errno));
					exit(0);
				}*/

				// send data to client
				if (write(connectfd, "A\n", 2) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}

				/*
				if (shutdown(connectfd, SHUT_RDWR) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					exit(1);
				}*/

				free(buf);
				close(connectfd);
				//close(sockfd);

				return 0;
			}
		}

		memset(&input, '\0', sizeof(input));
	}
	
	return 0;
}

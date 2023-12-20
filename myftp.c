#include "myftp.h"

void runCD(char* path) {
	if (path != NULL) {
		
		// check if user can access directory
		if (access(path, R_OK) == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
		} else {
			chdir(path); // could error check this. But if this fails, there are bigger problems

			if (DEBUG) {
				printf("Changed directory to %s\n", path);
			}
		}

	} else {
		printf("Failed to specify a pathname\n");
	}
	return;
}

void runLS() {
	int pid = fork();

	if (pid < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		return;
	}

	if (!pid) {
		//child

		// setting up pipe
		int fd[2];
		int rdr, wtr = 0;

		if (pipe(fd) != 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(0);
		}

		rdr = fd[0];
		wtr = fd[1];

		int pid2 = fork();

		if (pid2 < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(0);
		}

		if (pid2) {
			// child's parent
			close(wtr);

			// make the file descriptor rdr the new stdin
			close(0);
			dup(rdr);
			close(rdr);

			if (execlp("more", "more", "-20", NULL) == -1) { 
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1);
			}

			// end process
			exit(0);
		} else {
			// child's child
			close(rdr);

			// make the file descriptor wtr the new stout
			close(1);
			dup(wtr);
			close(wtr);

			if (execlp("ls", "ls", "-l", NULL) == -1) { 
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1);
			}

			// end process
			close(wtr);
			exit(0);
		}

		// end process
		exit(0);

	}

	// parent waits for children to finish (no zombies/orphans)
	int status;
	waitpid(pid, &status, 0);
	// something for status here?

	return;
}

void showContents(int readfd) {
	int pid = fork();

	if (pid < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		return;
	}

	if (!pid) {
		//child

		// setting up pipe
		int fd[2];
		int wtr = 0;

		if (pipe(fd) != 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(0);
		}

		wtr = fd[1];

		// child's parent
		close(wtr);

		// make the file descriptor rdr the new stdin
		close(0);
		dup(readfd);
		close(readfd);

		if (execlp("more", "more", "-20", NULL) == -1) { 
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}

		// end process
		close(readfd);
		exit(0);

	}

	// parent waits for children to finish (no zombies/orphans)
	int status;
	waitpid(pid, &status, 0);
	// something for status here?

	return;
}

int getDataSocketConnection(int sockfd) {
	// send server command
	if (write(sockfd, "D\n", 2) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}

	// init vars
	char *buf = calloc(BUF_SIZE, sizeof(char));

	int err, dataSockfd, bRead;
	struct addrinfo hints, *dataSockAddr;
	memset(&hints, 0, sizeof(hints));

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;

	// get server response
	if (read(sockfd, buf, 1) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		free(buf);
		return -1;
	}
				
	if (strcmp(buf, "E") == 0) {
		if (read(sockfd, buf, (BUF_SIZE-1)) < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			//exit(0);
		}
		printf("%s", buf);
		free(buf);
		return -1;
	} else {

		if (strcmp(buf, "A") == 0) {
			// get data socket port (size is 16 + 1 for '\n')
			if ((bRead = read(sockfd, buf, BUF_SIZE)) < 0) {
				fprintf(stderr, "%s\n", strerror(errno));
				free(buf);
				return -1;
			}

			buf[bRead-1] = '\0';

			if (DEBUG) {
				printf("Data socket made on port %s\n", buf);
			}

			// connecting to socket
			if ((err = getaddrinfo(NULL, buf, &hints, &dataSockAddr)) != 0) {
				fprintf(stderr, "%s\n", gai_strerror(err));
				free(buf);
				return -1;
			}
						
			if ((dataSockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				fprintf(stderr, "%s\n", strerror(errno));
				free(buf);
				return -1;
			}

			setsockopt(dataSockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
						
			if ((connect(dataSockfd, dataSockAddr->ai_addr, dataSockAddr->ai_addrlen)) != 0) {
				fprintf(stderr, "%s\n", strerror(errno));
				free(buf);
				return -1;
			}
		}
	}
	
	free(buf);
	return dataSockfd;
}

int main(int argc, char *argv[]) {
	
	// getting port number to connect to
	char *portNum = malloc(10*sizeof(char));

	if (argc > 2) {
		printf("Invalid number of arguments\n");
		return 1;
	}

	if (argc == 2) {
		strcpy(portNum, argv[1]);
	} else {
		strcpy(portNum, PORT);
	}

	if (DEBUG) {
		printf("PORT: %s\n", portNum);
	}

	// setting up command socket
	int sockfd, err;
	struct addrinfo hints, *servaddr;
	memset(&hints, 0, sizeof(hints));

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;

	if ((err = getaddrinfo(NULL, PORT, &hints, &servaddr)) != 0) {
		fprintf(stderr, "%s\n", gai_strerror(err));
		exit(0);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	
	// connecting to server
	if ((connect(sockfd, servaddr->ai_addr, servaddr->ai_addrlen)) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(0);
	}

	// print username
	uid_t uid = geteuid();
	errno = 0;
	struct passwd *pwd = getpwuid(uid);

	if (pwd == NULL) {
		if (errno != 0) {
			perror(strerror(errno));
		} else {
			printf("Username not found\n");
		}
	} else {
		printf("Welcome %s!\n", pwd->pw_name);
	}

	// init vars for main loop
	char *buf = malloc(BUF_SIZE * sizeof(char));	
	char *input = calloc(BUF_SIZE, sizeof(char));
	char *tok = NULL;

	int bRead, dataSockfd, fd;

	while (1) {

		// get user input
		if ((bRead = read(0, input, BUF_SIZE)) < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			break;
		}

		// tokenize input
		input[bRead-1] = '\0';
		tok = strtok(input, " ");
		
		// stops client and closes its server proccess 
		if ((tok != NULL) && (strcmp(tok, "exit") == 0)) {
			break;
		}

		// change directory for client
		if ((tok != NULL) && (strcmp(tok, "cd") == 0)) {
			tok = strtok(NULL, " ");
			runCD(tok);
		}
		
		// change directory for server
		if ((tok != NULL) && (strcmp(tok, "rcd") == 0)) {
			tok = strtok(NULL, " ");

			if (tok != NULL) {
				// send command and pathname to server
				if (write(sockfd, "C", 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}

				if (write(sockfd, tok, BUF_SIZE) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}

				if (write(sockfd, "\n", 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}

				memset(buf, '\0', BUF_SIZE * sizeof(char));

				// get server response
				if (read(sockfd, buf, 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}
				
				if (strcmp(buf, "E") == 0) {
					if (read(sockfd, buf, (BUF_SIZE-1)) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}
					printf("%s", buf);
				} else {

					if ((strcmp(buf, "A") == 0) && DEBUG) {
						printf("Server directory changed to %s\n", tok);
					}

					if (read(sockfd, buf, 1) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}
				}
			} else {
				printf("Failed to specify a pathname\n");
			}
			
		}

		// list files on client side
		if ((tok != NULL) && (strcmp(tok, "ls") == 0)) {
			runLS();
		}

		// list files on server side
		if ((tok != NULL) && (strcmp(tok, "rls")) == 0) {

			dataSockfd = getDataSocketConnection(sockfd);
			if (dataSockfd > 0) {

				// send command to server
				if (write(sockfd, "L\n", 2) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}

				memset(buf, '\0', BUF_SIZE * sizeof(char));
				
				if (read(sockfd, buf, 1) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
					//exit(0);
				}
				
				if (strcmp(buf, "E") == 0) {
					if (read(sockfd, buf, (BUF_SIZE-1)) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}
					printf("%s", buf);
				} else {
					if (strcmp(buf, "A") == 0) {
						if (read(sockfd, buf, 1) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}

						showContents(dataSockfd);
					}
				}
				
				/*
				// print data sent by data socket from server
				while ((bRead = read(dataSockfd, buf, BUF_SIZE)) > 0) {
					printf("%s\n", buf);
					//fflush(stdout);
					if (bRead < BUF_SIZE) {
						break;
					}
				}*/
				//showContents(dataSockfd);
				
				close(dataSockfd);
			}
		}

		// get file from server and store it on client side
		if ((tok != NULL) && (strcmp(tok, "get") == 0)) {
			tok = strtok(NULL, " ");

			if (tok != NULL) {
				dataSockfd = getDataSocketConnection(sockfd);
				if (dataSockfd > 0) {

					// send command and pathname to server
					if (write(sockfd, "G", 1) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}

					if (write(sockfd, tok, BUF_SIZE) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}

					if (write(sockfd, "\n", 1) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}

					memset(buf, '\0', BUF_SIZE * sizeof(char));

					// get server response
					if (read(sockfd, buf, 1) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}
					
					if (strcmp(buf, "E") == 0) {
						if (read(sockfd, buf, (BUF_SIZE-1)) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}
						printf("%s", buf);
					} else {
						if (strcmp(buf, "A") == 0) {
							if (read(sockfd, buf, 1) < 0) {
								fprintf(stderr, "%s\n", strerror(errno));
								//exit(0);
							}

							// create file
							if ((fd = open(tok, O_CREAT | O_EXCL | O_WRONLY, NEW_FILE_PERMS)) < 0) {
								fprintf(stderr, "%s\n", strerror(errno));
							}
							
							if (DEBUG) {
								printf("Reading server and writing contents to %s\n", tok);
							}

							// write data to new file
							while ((bRead = read(dataSockfd, buf, BUF_SIZE)) > 0) {
								printf("%s\n", buf);
								if (write(fd, buf, bRead) < 0) {
									fprintf(stderr, "%s\n", strerror(errno));
									//exit(0);
								}

								if (bRead < BUF_SIZE) {
									break;
								}
							}
							close(fd);
						}
					}
					close(dataSockfd);
				}
			} else {
				printf("Failed to specify a pathname\n");
			}
		}

		// show contents of file on server side
		if ((tok != NULL) && (strcmp(tok, "show") == 0)) {
			tok = strtok(NULL, " ");

			if (tok != NULL) {
				dataSockfd = getDataSocketConnection(sockfd);
				if (dataSockfd > 0) {

					// send command and pathname to server
					if (write(sockfd, "G", 1) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}

					if (write(sockfd, tok, BUF_SIZE) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}

					if (write(sockfd, "\n", 1) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}

					memset(buf, '\0', BUF_SIZE * sizeof(char));

					// get server response
					if (read(sockfd, buf, 1) < 0) {
						fprintf(stderr, "%s\n", strerror(errno));
						//exit(0);
					}
					
					if (strcmp(buf, "E") == 0) {
						if (read(sockfd, buf, (BUF_SIZE-1)) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}
						printf("%s", buf);
					} else {
						if (strcmp(buf, "A") == 0) {
							if (read(sockfd, buf, 1) < 0) {
								fprintf(stderr, "%s\n", strerror(errno));
								//exit(0);
							}

							showContents(dataSockfd);
						}
					}
					close(dataSockfd);
				}
			} else {
				printf("Failed to specify a pathname\n");
			}
		}

		// get file from client and store it on server side
		if ((tok != NULL) && (strcmp(tok, "put") == 0)) {
			tok = strtok(NULL, " ");

			if (tok != NULL) {
				// check if file is readable
				if ((fd = open(tok, O_RDONLY)) < 0) {
					fprintf(stderr, "%s\n", strerror(errno));
				} else {
					dataSockfd = getDataSocketConnection(sockfd);
					if (dataSockfd > 0) {

						// send command and pathname to server
						if (write(sockfd, "P", 1) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}

						if (write(sockfd, tok, BUF_SIZE) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}

						if (write(sockfd, "\n", 1) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}

						memset(buf, '\0', BUF_SIZE * sizeof(char));

						// get server response
						if (read(sockfd, buf, 1) < 0) {
							fprintf(stderr, "%s\n", strerror(errno));
							//exit(0);
						}
						
						if (strcmp(buf, "E") == 0) {
							if (read(sockfd, buf, (BUF_SIZE-1)) < 0) {
								fprintf(stderr, "%s\n", strerror(errno));
								//exit(0);
							}
							printf("%s", buf);
						} else {
							if (strcmp(buf, "A") == 0) {
								if (read(sockfd, buf, 1) < 0) {
									fprintf(stderr, "%s\n", strerror(errno));
									//exit(0);
								}

								if (DEBUG) {
									printf("Reading %s's contents and writing to server\n", tok);
								}

								// sending file's contents to server
								while ((bRead = read(fd, buf, BUF_SIZE)) > 0) {
									if (write(dataSockfd, buf, bRead) < 0) {
										fprintf(stderr, "%s\n", strerror(errno));
										//exit(0);
									}

									if (bRead < BUF_SIZE) {
										break;
									}
								}
							}
						}
						close(dataSockfd);
					}
					close(fd);
				}
			} else {
				printf("Failed to specify a pathname\n");
			}
		}

		// empty input vars
		memset(input, '\0', BUF_SIZE * sizeof(char));
		tok = NULL;

	}

	// send command to server
	if (write(sockfd, "Q\n", 2) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		//exit(0);
	}

	memset(buf, '\0', BUF_SIZE * sizeof(char));

	// get server response
	if (read(sockfd, buf, 1) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		//exit(0);
	}

	if (buf[0] == 'E') {
		if (read(sockfd, buf, (BUF_SIZE-1)) < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			//exit(0);
		}
		printf("%s", buf);
	}

	if (DEBUG && buf[0] == 'A') {
		printf("Server child process closed\n");
	}

	close(sockfd);

	// cleaning up memory
	free(buf);
	free(input);
	free(portNum);
	return 0;
		
}
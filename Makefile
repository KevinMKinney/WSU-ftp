.SUFFIXES: .c .o
CCFLAGS = -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE

OBJS = myftpserve.o myftp.o
OPTIONS = -g

all: ${OBJS} build

myftpserve.o: myftpserve.c myftp.h
	gcc ${CCFLAGS} ${OPTIONS} -c myftpserve.c
	
myftp.o: myftp.c myftp.h
	gcc ${CCFLAGS} ${OPTIONS} -c myftp.c

build: ${OBJS}
	gcc ${CCFLAGS} ${OPTIONS} -o myftpserve myftpserve.o
	gcc ${CCFLAGS} ${OPTIONS} -o myftp myftp.o

clean:
	rm -f myftpserve myftpserve.o
	rm -f myftp myftp.o
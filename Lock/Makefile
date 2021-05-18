CC=gcc
CFLAGS=-g 
HOME=/home/chenxuqiang/Tools/zookeeper-release-3.6.3/zookeeper-client/zookeeper-client-c/target/c
ZOOKEEPER_INC=-I${HOME}/include/zookeeper
ZOOKEEPER_LIB= -L${HOME}/lib -lzookeeper_mt

APP=mylock
all:
	${CC} lock.c -DTHREAD ${CFLAGS} ${ZOOKEEPER_INC} ${ZOOKEEPER_LIB} -o ${APP} 
clean:
	rm -f ${APP}

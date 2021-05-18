CC=gcc
HOME=/home/chenxuqiang/Tools/zookeeper-release-3.6.3/zookeeper-client/zookeeper-client-c/target/c
CFLAGS=-g 
ZOOKEEPER_INC=-I${HOME}/include/zookeeper
ZOOKEEPER_LIB= -L${HOME}/lib -lzookeeper_mt

all: monitor ClusterMonitor

monitor: monitor.c
	${CC} monitor.c -DTHREAD ${CFLAGS} ${ZOOKEEPER_INC} ${ZOOKEEPER_LIB} -o monitor 

ClusterMonitor: ClusterMonitor.c
	${CC} ClusterMonitor.c -DTHREAD ${CFLAGS} ${ZOOKEEPER_INC} ${ZOOKEEPER_LIB} -o ClusterMonitor

clean:
	rm -f monitor
	rm -f ClusterMonitor

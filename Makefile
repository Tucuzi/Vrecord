APP=vrecord
APP2=vrmanager

OBJECT= vrecord.o mx6H264Source.o mx6H264MediaSubsession.o
COBJECT= fb.o utils.o parse.o mp4mux.o capture.o vencode.o
COMMON_OBJECT=parse.o utils.o
OBJECT2=vrmanager.o 

LIVE555_DIR=/home/neero/project/3g-vedio-cam/opensource/live555/out/usr/local
LIVE555_LIB=-L${LIVE555_DIR}/lib -lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment
LIVE555_HEADER=-I${LIVE555_DIR}/include/BasicUsageEnvironment -I${LIVE555_DIR}/include/groupsock -I${LIVE555_DIR}/include/liveMedia -I${LIVE555_DIR}/include/UsageEnvironment

CFLAGS=-Wall -O3
#CFLAGS=-O2
HEADER=${LIVE555_HEADER}
LDFLAGS=-lvpu -lmp4v2 -lpthread ${LIVE555_LIB}

CROSS_COMPILER=/home/neero/project/3g-vedio-cam/workspace/sdk/x86_64-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi
CROSS_OPTS=--sysroot=/home/neero/project/3g-vedio-cam/workspace/sdk/sysroot -march=armv7-a -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9
CROSS_GCC=${CROSS_COMPILER}-gcc ${CROSS_OPTS}
CROSS_GPP=${CROSS_COMPILER}-g++ ${CROSS_OPTS}

.PHONY: all clean
all: ${OBJECT} ${OBJECT2} ${COBJECT}
	${CROSS_GPP} -o ${APP} ${OBJECT} ${COBJECT} ${LDFLAGS} 
	${CROSS_GPP} -o ${APP2} ${OBJECT2} ${COMMON_OBJECT} -lpthread

${OBJECT}:%.o: %.cpp
	${CROSS_GPP} ${CFLAGS} ${HEADER} -c $<

${OBJECT2}:%.o: %.cpp
	${CROSS_GPP} ${CFLAGS} ${HEADER} -c $<

${COBJECT}:%.o: %.c
	${CROSS_GCC} ${CFLAGS} ${HEADER} -c $<


clean:
	-rm ${APP} ${OBJECT} $(COBJECT)
	-rm ${APP2} ${OBJECT2}

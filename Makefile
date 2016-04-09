APP=vrecord
APP2=vrmanager

OBJECT= vrecord.o 
COBJECT= fb.o utils.o parse.o mp4mux.o capture.o vencode.o
COMMON_OBJECT=parse.o utils.o
OBJECT2=vrmanager.o 

CFLAGS=-Wall -O2
#CFLAGS=-O2
HEADER=
LDFLAGS=-lvpu -lmp4v2 -lpthread

CROSS_COMPILER=/home/neero/project/3g-vedio-cam/workspace/sdk/x86_64-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi
CROSS_OPTS=--sysroot=/home/neero/project/3g-vedio-cam/workspace/sdk/sysroot -march=armv7-a -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9
CROSS_GCC=${CROSS_COMPILER}-gcc ${CROSS_OPTS}
CROSS_GPP=${CROSS_COMPILER}-g++ ${CROSS_OPTS}

.PHONY: all clean
all: ${OBJECT} ${OBJECT2} ${COBJECT}
	${CROSS_GPP} -o ${APP} ${LDFLAGS} ${OBJECT} ${COBJECT}
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

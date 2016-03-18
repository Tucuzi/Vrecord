APP=vrecord
APP2=vrmanager

OBJECT= utils.o vencode.o vrecord.o mp4mux.o fb.o capture.o parse.o
COMMON_OBJECT=parse.o utils.o
OBJECT2=vrmanager.o 

CFLAGS=-Wall -O3
HEADER=
LDFLAGS=-lvpu -lmp4v2 -lpthread

CROSS_COMPILER=/home/neero/project/3g-vedio-cam/workspace/sdk/x86_64-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi

CROSS_GCC=${CROSS_COMPILER}-gcc --sysroot=/home/neero/project/3g-vedio-cam/workspace/sdk/sysroot -march=armv7-a -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 

.PHONY: all clean
all: ${OBJECT} $(OBJECT2)
	${CROSS_GCC} -o ${APP} ${LDFLAGS} ${OBJECT}
	${CROSS_GCC} -o ${APP2} ${OBJECT2} ${COMMON_OBJECT} -lpthread

${OBJECT}:%.o: %.c
	${CROSS_GCC} ${CFLAGS} ${HEADER} -c $<

${OBJECT2}:%.o: %.c
	${CROSS_GCC} ${CFLAGS} ${HEADER} -c $<

clean:
	-rm ${APP} ${OBJECT}
	-rm ${APP2} ${OBJECT2}

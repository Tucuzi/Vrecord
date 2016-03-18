#!/bin/sh

TIME_STAMP=`date +"%Y-%m-%d_%H-%M"`
time=$(date +"%Y-%m-%d_%H:%M")
FILE=vrecord
BK_FILE="capture.c fb.c utils.c vencode.c parse.c vrecord.c mp4mux.c vrmanager.c parse.h vpu_jpegtable.h vrecord.h Makefile vrecord.conf README"
mkdir $FILE
cp ${BK_FILE} $FILE

TAR_BKG="$FILE"_$TIME_STAMP.tar.bz2
#echo $TAR_BKG
tar cjf $TAR_BKG $FILE
rm -rf $FILE

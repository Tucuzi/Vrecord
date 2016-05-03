#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include <assert.h>  

#include <sys/types.h>
#include <sys/syscall.h>

#include "mx6H264MediaSubsession.hpp"
#include "mx6H264Source.hpp"

#define FRAME_PER_SEC 25

int mpSourceInit = 0;
int mVdataSyncFlag = 0;
int mpConnect = 0;

pid_t gettid()  
{  
    return syscall(SYS_gettid);  
}  

mx6H264MediaSubsession* mx6H264MediaSubsession::createNew (UsageEnvironment &env, FramedSource *source, void *ptr)  
{  
    return new mx6H264MediaSubsession(env, source, ptr);  
}  
      
mx6H264MediaSubsession::mx6H264MediaSubsession (UsageEnvironment &env, FramedSource *source, void* ptr)  
        : OnDemandServerMediaSubsession(env, True) // reuse the first source  
{  
    fprintf(stderr, "[%d] %s .... calling\n", gettid(), __func__);  
    mpSource = source;  
    mpSdpLine = 0;
    mpPtr = ptr;
}  
      
mx6H264MediaSubsession::~mx6H264MediaSubsession ()  
{  
    fprintf(stderr, "[%d] %s .... calling\n", gettid(), __func__);  
    if (mpSdpLine) 
        free(mpSdpLine);  
}  
      
void mx6H264MediaSubsession::afterPlayingDummy (void *ptr)  
{  
    fprintf(stderr, "[%d] %s .... calling\n", gettid(), __func__);  

    mx6H264MediaSubsession *This = (mx6H264MediaSubsession*)ptr;  
    This->mpDone = 0xff;  
}  
      
void mx6H264MediaSubsession::chkForAuxSDPLine (void *ptr)  
{  
    mx6H264MediaSubsession *This = (mx6H264MediaSubsession *)ptr;  
    This->chkForAuxSDPLine1();  
}  
      
void mx6H264MediaSubsession::chkForAuxSDPLine1 ()  
{  
    fprintf(stderr, "[%d] %s .... calling\n", gettid(), __func__);  
    if (mpDummyRtpsink->auxSDPLine()) {  
        printf("chkForAuxSDPLine1 finish\n");
        mpDone = 0xff;  
    }
    else {  
        double delay = 1000.0 / (FRAME_PER_SEC);//ms
        int to_delay = delay*1000;   // us  
        nextTask() = envir().taskScheduler().scheduleDelayedTask(to_delay,  
            chkForAuxSDPLine, this);  
    }  
}  
  
const char * mx6H264MediaSubsession::getAuxSDPLine (RTPSink *sink, FramedSource *source)  
{  
    fprintf(stderr, "[%d] %s .... calling\n", gettid(), __func__);  
    if (mpSdpLine) return mpSdpLine;  
    
    //mpSourceInit = 1;
    mpDummyRtpsink = sink;  
    mpDummyRtpsink->startPlaying(*source, 0, 0);  
    //mpDummyRtpsink->startPlaying(*source, afterPlayingDummy, this);  
    chkForAuxSDPLine(this); 
    //printf("start play\n"); 
    mpDone = 0;  
    envir().taskScheduler().doEventLoop(&mpDone);  
    mpSdpLine = strdup(mpDummyRtpsink->auxSDPLine());  
    mpDummyRtpsink->stopPlaying();  
     
    //printf("The auxSDPLine is %s\n",mpSdpLine);
    return mpSdpLine;  
}  
      
RTPSink * mx6H264MediaSubsession::createNewRTPSink(Groupsock *rtpsock, unsigned char type, FramedSource *source)  
{  
    fprintf(stderr, "[%d] %s .... calling\n", gettid(), __func__);  
  
    return H264VideoRTPSink::createNew(envir(), rtpsock, type);  
}  
      
FramedSource * mx6H264MediaSubsession::createNewStreamSource (unsigned sid, unsigned &bitrate)  
{  
    fprintf(stderr, "[%d] %s .... calling\n", gettid(), __func__);
    mpConnect++;
    mVdataSyncFlag = 1;

    if (mpConnect != 2) {
        mpSourceInit = 1;
    }
  
    bitrate = 500;  
    return H264VideoStreamFramer::createNew(envir(), new mx6H264Source(envir(), mpPtr));  
}  


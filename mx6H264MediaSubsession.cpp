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

mx6H264MediaSubsession* mx6H264MediaSubsession::createNew (UsageEnvironment &env, FramedSource *source, void *ptr)  
{  
    return new mx6H264MediaSubsession(env, source, ptr);  
}  
      
mx6H264MediaSubsession::mx6H264MediaSubsession (UsageEnvironment &env, FramedSource *source, void* ptr)  
        : OnDemandServerMediaSubsession(env, True) // reuse the first source  
{  
    fprintf(stderr, "[%d] %s .... calling\n", getpid(), __func__);  
    mp_source = source;  
    mp_sdp_line = 0;
    mp_ptr = ptr;
}  
      
mx6H264MediaSubsession::~mx6H264MediaSubsession ()  
{  
    fprintf(stderr, "[%d] %s .... calling\n", getpid(), __func__);  
    if (mp_sdp_line) free(mp_sdp_line);  
}  
      
void mx6H264MediaSubsession::afterPlayingDummy (void *ptr)  
{  
    fprintf(stderr, "[%d] %s .... calling\n", getpid(), __func__);  

    mx6H264MediaSubsession *This = (mx6H264MediaSubsession*)ptr;  
    This->m_done = 0xff;  
}  
      
void mx6H264MediaSubsession::chkForAuxSDPLine (void *ptr)  
{  
    mx6H264MediaSubsession *This = (mx6H264MediaSubsession *)ptr;  
    This->chkForAuxSDPLine1();  
}  
      
void mx6H264MediaSubsession::chkForAuxSDPLine1 ()  
{  
    fprintf(stderr, "[%d] %s .... calling\n", getpid(), __func__);  
    if (mp_dummy_rtpsink->auxSDPLine())  
        m_done = 0xff;  
    else {  
        int delay = 100*1000;   // 100ms  
        nextTask() = envir().taskScheduler().scheduleDelayedTask(delay,  
            chkForAuxSDPLine, this);  
    }  
}  
  
const char * mx6H264MediaSubsession::getAuxSDPLine (RTPSink *sink, FramedSource *source)  
{  
    fprintf(stderr, "[%d] %s .... calling\n", getpid(), __func__);  
    if (mp_sdp_line) return mp_sdp_line;  
    
    mp_dummy_rtpsink = sink;  
    mp_dummy_rtpsink->startPlaying(*source, 0, 0);  
    //mp_dummy_rtpsink->startPlaying(*source, afterPlayingDummy, this);  
    chkForAuxSDPLine(this);  
    m_done = 0;  
    envir().taskScheduler().doEventLoop(&m_done);  
    mp_sdp_line = strdup(mp_dummy_rtpsink->auxSDPLine());  
    mp_dummy_rtpsink->stopPlaying();  
      
    return mp_sdp_line;  
}  
      
RTPSink * mx6H264MediaSubsession::createNewRTPSink(Groupsock *rtpsock, unsigned char type, FramedSource *source)  
{  
    fprintf(stderr, "[%d] %s .... calling\n", getpid(), __func__);  
    return H264VideoRTPSink::createNew(envir(), rtpsock, type);  
}  
      
FramedSource * mx6H264MediaSubsession::createNewStreamSource (unsigned sid, unsigned &bitrate)  
{  
    fprintf(stderr, "[%d] %s .... calling\n", getpid(), __func__);  
    bitrate = 500;  
    return H264VideoStreamFramer::createNew(envir(), new mx6H264Source(envir(), mp_ptr));  
}  


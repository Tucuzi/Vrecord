#ifndef _MX6H264MEDIASUBSESSION_
#define _MX6H264MEDIASUBSESSION_

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>

class mx6H264MediaSubsession : public OnDemandServerMediaSubsession  
{  
public:  
    static mx6H264MediaSubsession *createNew (UsageEnvironment &env, FramedSource *source, void *ptr);  

protected:  
    mx6H264MediaSubsession (UsageEnvironment &env, FramedSource *source, void* ptr);
    ~mx6H264MediaSubsession ();
      
private:  
    static void afterPlayingDummy (void *ptr); 
    static void chkForAuxSDPLine (void *ptr);
    void chkForAuxSDPLine1 ();
     
protected:  
    virtual const char *getAuxSDPLine (RTPSink *sink, FramedSource *source);
    virtual RTPSink *createNewRTPSink(Groupsock *rtpsock, unsigned char type, FramedSource *source);   
    virtual FramedSource *createNewStreamSource (unsigned sid, unsigned &bitrate);  

private:  
    FramedSource *mpSource;    // \u5bf9\u5e94 WebcamFrameSource  
    char *mpSdpLine;
    void *mpPtr;  
    RTPSink *mpDummyRtpsink;  
    char mpDone; 
//    int mpConnect; 
};  
 
#endif

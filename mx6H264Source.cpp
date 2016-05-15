#include "vrecord.hpp"
#include "mx6H264Source.hpp"
#include <GroupsockHelper.hh> // for "gettimeofday()"

#define FRAME_PER_SEC 25
extern int mpSourceInit;
static char psp[] = {0x0, 0x0, 0x0, 0x1, 0x67, 0x42, 0x40, 0x1e, 0xa6, 0x80, 0xb4, 0x12, 0x64};
static char pps[] = {0x0, 0x0, 0x0, 0x1, 0x68, 0xce, 0x30, 0xa4, 0x80};

extern int mVdataSyncFlag;
extern int mpConnect;

mx6H264Source* mx6H264Source::createNew(UsageEnvironment& env, void *ptr) {
    printf("mx6H264Source::%s\n",__func__);
    return new mx6H264Source(env, ptr);
}

EventTriggerId mx6H264Source::eventTriggerId = 0;

unsigned mx6H264Source::referenceCount = 0;

mx6H264Source::mx6H264Source(UsageEnvironment& env, void *ptr)
    //:H264or5VideoStreamFramer(264, env, NULL, False, False)
    : FramedSource(env) 
{
   //printf("mx6H264Source::%s\n",__func__);
    eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
    referenceCount = 0;
    mpDisconnect = 0;
    mpPtr = ptr;
    mStep=0;
}

mx6H264Source::~mx6H264Source() {
    envir().taskScheduler().deleteEventTrigger(eventTriggerId);
    eventTriggerId = 0;
}

#if 1
void mx6H264Source::doStopGettingFrames()
{
    struct video_record *vp;   

    printf("mx6H264Source::%s\n",__func__);
    vp = (struct video_record *) mpPtr;

    mpDisconnect++;
#if 0
    if (mpConnect >= 2) {
        mVdataSyncFlag = 0;
        vp->enc.outtail = vp->enc.outhead;
    }
#endif
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
}
#endif

void mx6H264Source::doGetNextFrame()
{
    deliverFrame();
}

void mx6H264Source::deliverFrame0(void * clientData)
{
    ((mx6H264Source*)clientData)->deliverFrame();
}

void mx6H264Source::deliverFrame()
{
    int i;
    int old_tail;
    char *outbuf;
    struct video_record *vp;
    
    vp = (struct video_record *) mpPtr;
    old_tail = vp->enc.outtail;

    
    if (vp->enc.outtail == vp->enc.outhead) {
        //printf("overflaw\n");
        usleep(1000*1000/25);
    }

    outbuf = vp->enc.output[vp->enc.outtail].data;
    
    gettimeofday(&fPresentationTime, 0);  
    fFrameSize = vp->enc.output[vp->enc.outtail++].len;
    
    if(vp->enc.outtail == MAX_BUF_NUM)
        vp->enc.outtail = 0;

#if 0
    if (vp->enc.outtail == vp->enc.outhead) {
        vp->enc.outtail = vp->enc.outtail-1;
        if(vp->enc.outtail < 0)
            vp->enc.outtail = MAX_BUF_NUM-1;
    }
#else
    if (vp->enc.outtail == vp->enc.outhead)
        vp->enc.outtail = old_tail;
#endif

    //if(mpSourceInit>1)
        //mStep = 2;
#if 0   
    switch(mStep) {
        case 0:
            outbuf = psp;
            fFrameSize = sizeof(psp)/sizeof(psp[0]);

           // memmove(fTo, outbuf, fFrameSize);
            mStep = 1;
            break;

        case 1:
            outbuf = pps;
            fFrameSize = sizeof(pps)/sizeof(pps[0]);
            //memmove(fTo, outbuf, fFrameSize);
            mStep = 2;
            break;

        default:
            break;
    }  
#endif

    if (fFrameSize > fMaxSize) {  
        fNumTruncatedBytes = fFrameSize - fMaxSize;  
        fFrameSize = fMaxSize;  
    }  
    else {  
         fNumTruncatedBytes = 0;  
    }  
  
    //info_msg("fFramSize = %d\n", fFrameSize);
    
    memmove(fTo, outbuf, fFrameSize);
    // notify 
    afterGetting(this);   
}  

#if 1
unsigned mx6H264Source::maxFrameSize() const
{
    return 140*1024;
}
#endif


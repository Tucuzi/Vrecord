#include "vrecord.hpp"
#include "mx6H264Source.hpp"
#include <GroupsockHelper.hh> // for "gettimeofday()"

#define FRAME_PER_SEC 25

mx6H264Source* mx6H264Source::createNew(UsageEnvironment& env, void *ptr) {
    //printf("mx6H264Source::%s\n",__func__);
    return new mx6H264Source(env, ptr);
}

EventTriggerId mx6H264Source::eventTriggerId = 0;

unsigned mx6H264Source::referenceCount = 0;

mx6H264Source::mx6H264Source(UsageEnvironment& env, void *ptr)
    : FramedSource(env) 
{
#if 0
  if (referenceCount == 0) {
    // Any global initialization of the device would be done here:
    //%%% TO BE WRITTEN %%%
  }
  ++referenceCount;
    printf("mx6H264Source::%s\n",__func__);
  // Any instance-specific initialization of the device would be done here:
  //%%% TO BE WRITTEN %%%

  // We arrange here for our "deliverFrame" member function to be called
  // whenever the next frame of data becomes available from the device.
  //
  // If the device can be accessed as a readable socket, then one easy way to do this is using a call to
  //     envir().taskScheduler().turnOnBackgroundReadHandling( ... )
  // (See examples of this call in the "liveMedia" directory.)
  //
  // If, however, the device *cannot* be accessed as a readable socket, then instead we can implement it using 'event triggers':
  // Create an 'event trigger' for this device (if it hasn't already been done):
  if (eventTriggerId == 0) {
    eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
  }
#endif 
    m_started = 0;  
    mp_token = 0;
    mp_ptr = ptr;
}

mx6H264Source::~mx6H264Source() {
  // Any instance-specific 'destruction' (i.e., resetting) of the device would be done here:
  //%%% TO BE WRITTEN %%%
#if 0
  --referenceCount;
  if (referenceCount == 0) {
    // Any global 'destruction' (i.e., resetting) of the device would be done here:
    //%%% TO BE WRITTEN %%%

    // Reclaim our 'event trigger'
    envir().taskScheduler().deleteEventTrigger(eventTriggerId);
    eventTriggerId = 0;
  }
#endif
}

void mx6H264Source::doGetNextFrame() {
#if 0
  // This function is called (by our 'downstream' object) when it asks for new data.

  printf("%s\n",__func__);
  // Note: If, for some reason, the source device stops being readable (e.g., it gets closed), then you do the following:
  if (0 /* the source stops being readable */ /*%%% TO BE WRITTEN %%%*/) {
    handleClosure();
    return;
  }

  // If a new frame of data is immediately available to be delivered, then do this now:
  if (0 /* a new frame of data is immediately available to be delivered*/ /*%%% TO BE WRITTEN %%%*/) {
    deliverFrame();
  }

  // No new data is immediately available to be delivered.  We don't do anything more here.
  // Instead, our event trigger must be called (e.g., from a separate thread) when new data becomes available.
#endif
    //--------------------------------------------------------------------------------------------------------------//
   
    if (m_started) return;  
        m_started = 1;  
  
        // 根据 fps, 计算等待时间  
    double delay = 1000.0 / FRAME_PER_SEC;  
    int to_delay = delay * 1000;    // us  
  
    mp_token = envir().taskScheduler().scheduleDelayedTask(to_delay,  
        getNextFrame, this);
  
}

unsigned mx6H264Source::maxFrameSize() const
{
    return 500*1024;
}

void mx6H264Source::getNextFrame (void *ptr)  
{  
    ((mx6H264Source*)ptr)->getNextFrame1();  
}  
  
void mx6H264Source::getNextFrame1 ()  
{ 
    char *outbuf;
    struct video_record *vp;
    
    vp = (struct video_record *) mp_ptr;
    outbuf = vp->enc.output_ptr;

    gettimeofday(&fPresentationTime, 0);  
    fFrameSize = vp->enc.outlen;  
    if (fFrameSize > fMaxSize) {  
        fNumTruncatedBytes = fFrameSize - fMaxSize;  
        fFrameSize = fMaxSize;  
    }  
    else {  
         fNumTruncatedBytes = 0;  
    }  
  
    memmove(fTo, outbuf, fFrameSize);  
  
    // notify  
    afterGetting(this);   
    m_started = 0;  
}  

void mx6H264Source::deliverFrame0(void* clientData) {
  ((mx6H264Source*)clientData)->deliverFrame();
}

void mx6H264Source::deliverFrame() {
  // This function is called when new frame data is available from the device.
  // We deliver this data by copying it to the 'downstream' object, using the following parameters (class members):
  // 'in' parameters (these should *not* be modified by this function):
  //     fTo: The frame data is copied to this address.
  //         (Note that the variable "fTo" is *not* modified.  Instead,
  //          the frame data is copied to the address pointed to by "fTo".)
  //     fMaxSize: This is the maximum number of bytes that can be copied
  //         (If the actual frame is larger than this, then it should
  //          be truncated, and "fNumTruncatedBytes" set accordingly.)
  // 'out' parameters (these are modified by this function):
  //     fFrameSize: Should be set to the delivered frame size (<= fMaxSize).
  //     fNumTruncatedBytes: Should be set iff the delivered frame would have been
  //         bigger than "fMaxSize", in which case it's set to the number of bytes
  //         that have been omitted.
  //     fPresentationTime: Should be set to the frame's presentation time
  //         (seconds, microseconds).  This time must be aligned with 'wall-clock time' - i.e., the time that you would get
  //         by calling "gettimeofday()".
  //     fDurationInMicroseconds: Should be set to the frame's duration, if known.
  //         If, however, the device is a 'live source' (e.g., encoded from a camera or microphone), then we probably don't need
  //         to set this variable, because - in this case - data will never arrive 'early'.
  // Note the code below.

printf("deliverFrame\n");
  if (!isCurrentlyAwaitingData()) return; // we're not ready for the data yet

  u_int8_t* newFrameDataStart = (u_int8_t*)0xDEADBEEF; //%%% TO BE WRITTEN %%%
  unsigned newFrameSize = 0; //%%% TO BE WRITTEN %%%

  // Deliver the data here:
  if (newFrameSize > fMaxSize) {
    fFrameSize = fMaxSize;
    fNumTruncatedBytes = newFrameSize - fMaxSize;
  } else {
    fFrameSize = newFrameSize;
  }
  gettimeofday(&fPresentationTime, NULL); // If you have a more accurate time - e.g., from an encoder - then use that instead.
  // If the device is *not* a 'live source' (e.g., it comes instead from a file or buffer), then set "fDurationInMicroseconds" here.
  memmove(fTo, newFrameDataStart, fFrameSize);

  // After delivering the data, inform the reader that it is now available:
  FramedSource::afterGetting(this);
}


// The following code would be called to signal that a new frame of data has become available.
// This (unlike other "LIVE555 Streaming Media" library code) may be called from a separate thread.
// (Note, however, that "triggerEvent()" cannot be called with the same 'event trigger id' from different threads.
// Also, if you want to have multiple device threads, each one using a different 'event trigger id', then you will need
// to make "eventTriggerId" a non-static member variable of "mx6H264Source".)
void signalNewFrameData() {
  TaskScheduler* ourScheduler = NULL; //%%% TO BE WRITTEN %%%
  mx6H264Source* ourDevice  = NULL; //%%% TO BE WRITTEN %%%

  if (ourScheduler != NULL) { // sanity check
    ourScheduler->triggerEvent(mx6H264Source::eventTriggerId, ourDevice);
  }
}



#ifndef _MX6H264SOURCE_H_
#define _MX6H264SOURCE_H_

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

// The following class can be used to define specific encoder parameters

class mx6H264Source: public FramedSource {
public:
  static mx6H264Source* createNew(UsageEnvironment& env);

public:
  static EventTriggerId eventTriggerId;
  // Note that this is defined here to be a static class variable, because this code is intended to illustrate how to
  // encapsulate a *single* device - not a set of devices.
  // You can, however, redefine this to be a non-static member variable.
  
  mx6H264Source(UsageEnvironment& env);
  // called only by createNew(), or by subclass constructors
  ~mx6H264Source();

private:
  // redefined virtual functions:
  virtual void doGetNextFrame();
  //virtual void doStopGettingFrames(); // optional

private:
  static void deliverFrame0(void* clientData);
  void deliverFrame();

private:
  static unsigned referenceCount; // used to count how many instances of this class currently exist
};


#endif



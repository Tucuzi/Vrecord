#ifndef __MUXMKV_H__
#define   __MUXMKV_H__

//#include "libmkv.h"
extern mux_object_t *  MKVInit( char* vfile );
extern int8_t MKVEnd( mux_object_t *mux );
extern int8_t MKVaddFrame(mux_object_t *mux, int8_t bKeyFrame, uint64_t duration);
extern int8_t MKVappendFrameData(mux_object_t *mux, uint8_t *data, uint32_t dataSize );
#endif

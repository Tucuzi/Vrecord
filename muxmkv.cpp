#include <time.h>
#include "vrecord.hpp"

#define DEFAULT_STREAM_FPS  (25)
#define TIMECODE_SCALE 1000000

uint8_t codecPrivate[] = {
    0x01,0x64,0x00,0x2a,0xff,0xe1,0x00,0x0c,
    0x67,0x64,0x00,0x2a,0xac,0x2b,0x50,0x3c,
    0x01,0x13,0xf2,0xa0,0x01,0x00,0x04,0x68,
    0xee,0x3c,30
};

/**********************************************************************
 * MKVInit
 **********************************************************************
 * Allocates hb_mux_data_t structures, create file and write headers
 *********************************************************************/
mux_object_t *  MKVInit( char* vfile )
{
    mux_object_t *mux;

    mux = calloc(1, sizeof(mux_object_t));
    mux->config = calloc(1, sizeof(mk_TrackConfig));
    
    mux->writer = mk_createWriter(vfile, 1000000, 1);

    if( !mux->writer)
        return NULL;

    /* Set default video track configs */
	mux->config->trackType    = MK_TRACK_VIDEO;
	mux->config->flagEnabled  = 1;
	mux->config->flagDefault  = 1;
	mux->config->defaultDuration = 1000000 * 1000 / DEFAULT_STREAM_FPS;
	mux->config->name         = (char *)"video";
	mux->config->language     = (char *)"eng";
	mux->config->codecID      = (char *)MK_VCODEC_MP4AVC;
	mux->config->extra.video.flagInterlaced  = 0;
	mux->config->extra.video.pixelWidth      = PWIDTH;
	mux->config->extra.video.pixelHeight     = PHIGH;
	mux->config->extra.video.displayWidth    = PWIDTH;
	mux->config->extra.video.displayHeight   = PHIGH;
	mux->config->extra.video.displayUnit     = 0;
	mux->config->extra.video.aspectRatioType = MK_ASPECTRATIO_KEEP;
    mux->config->codecPrivateSize = sizeof(codecPrivate);
    mux->config->codecPrivate = calloc(mux->config->codecPrivateSize, 1);
    memcpy(mux->config->codecPrivate, codecPrivate, mux->config->codecPrivateSize);
    mux->track = mk_createTrack(mux->writer, mux->config);

    if( mk_writeHeader( mux->writer, PROGRM_CODE) < 0 )
    {
        err_msg( "Failed to write to output file, disk full?");
        free(mux->config);
        free(mux);
        return NULL;
    }
    
    return mux;
}

int8_t MKVaddFrame(mux_object_t *mux, int8_t bKeyFrame, uint64_t duration)
{
    int8_t rc = 0;
    struct timeval time;
    uint64_t timestamp;
    
    gettimeofday(&time, NULL);
	timestamp = time.tv_sec * 1000000 + time.tv_usec;
	//timestamp *= 1000;
	//printf("timestamp is %lld\n", timestamp);
	rc = mk_startFrame(mux->writer, mux->track) == 0;
    if (rc)
	    rc = mk_setFrameFlags(mux->writer, mux->track, timestamp, bKeyFrame, duration) == 0;

	return rc;
}

int8_t MKVappendFrameData(mux_object_t *mux, uint8_t *data, uint32_t dataSize )
{
	return mk_addFrameData(mux->writer, mux->track, data, dataSize) == 0;
}

int8_t MKVEnd( mux_object_t *mux )
{
    if( mk_close(mux->writer) < 0 )
        err_msg( "Failed to flush the last frame and close the output file, Disk Full?\n" );
    
    free(mux->config);
    free(mux);

    return 0;
}

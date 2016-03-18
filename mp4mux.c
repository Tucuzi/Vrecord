#include "vrecord.h"

extern int debug_fd;

static unsigned char sps[] ={
        0x67, 0x42, 0x40, 0x1e, 0xa6, 0x80, 0xb4, 0x12, 0x64}; 
        
static unsigned char pps[]= {
        0x68, 0xce, 0x30, 0xa4, 0x80};

int mp4mux_init(struct video_record *vrecord, char * vfile_name)
{
    vrecord->mp4_mux.mp4file = MP4CreateEx(vfile_name, 0, 1, 1, 0, 0, 0, 0);
    //file = MP4Create(vfile_name, 9);
    if (vrecord->mp4_mux.mp4file == MP4_INVALID_FILE_HANDLE){   
        err_msg("Create the mp4 file fialed.\n");
        return VR_MP4INIT_ERR;
    }

    MP4SetTimeScale(vrecord->mp4_mux.mp4file, 90000);
    vrecord->mp4_mux.video_track = MP4AddH264VideoTrack(vrecord->mp4_mux.mp4file, 
                                             90000, 90000/25, 
                                             vrecord->config->enc_width,
                                             vrecord->config->enc_height,
                                             sps[1], //sps[1] AVCProfileIndication
                                             sps[2], //sps[2] profile_compat
                                             sps[3], //sps[3] AVCLevelIndication
                                             3); // 4 bytes length before each NAL unit

    if (vrecord->mp4_mux.video_track == MP4_INVALID_TRACK_ID){
        err_msg("add video track failed.\n");
        return VR_MP4TRACK_ERR;
    }

#if 1
    MP4AddH264SequenceParameterSet(vrecord->mp4_mux.mp4file,
                                   vrecord->mp4_mux.video_track,
                                   sps,9);

    MP4AddH264PictureParameterSet(vrecord->mp4_mux.mp4file,
                                  vrecord->mp4_mux.video_track,
                                  pps,5);
#endif
    //MP4SetVideoProfileLevel(vrecord->mp4_mux.mp4file, 0x7F);
    MP4SetVideoProfileLevel(vrecord->mp4_mux.mp4file, 0x0F);
    //MP4SetVideoProfileLevel(vrecord->mp4_mux.mp4file, 0x1);

    return VR_OK;
}

int mp4_save_frame(struct video_record *vrecord, char *buf, int n, int syncframe)
{
    int nalsize = n-4;

    #if 1
    if (n < 20) {
        #if 0
		for (nalsize = 0; nalsize < n; nalsize++)
		    printf("0x%x ", buf[nalsize]);
		
		printf("\n");
        #endif
	}
	else
    #endif
    {
		#if 1
        buf[0] = (nalsize & 0xff000000) >> 24;  
        buf[1] = (nalsize & 0x00ff0000) >> 16;  
        buf[2] = (nalsize & 0x0000ff00) >> 8;  
        buf[3] =  nalsize & 0x000000ff;  
        #endif

		MP4WriteSample(vrecord->mp4_mux.mp4file, 
		    vrecord->mp4_mux.video_track, 
		    (u8 *)buf, n, MP4_INVALID_DURATION, 0, syncframe);

    }

    return n;
}


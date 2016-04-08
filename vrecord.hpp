#ifndef __VENCODE_H__
#define __VENCODE_H__

#include <linux/videodev2.h>
#include <errno.h>
#include <semaphore.h>
#include <vpu_lib.h>
#include <vpu_io.h>
#include <mp4v2/mp4v2.h>
//#include <uapi/linux/ipu.h>

//#include "libmkv.h"

#define COMMON_INIT

#define PROGRM_CODE     "vrecod"
#define PROGRM_VERSION  "0.1"

//#undef u64
#undef u32
#undef u16
#undef u8
//#undef s64
#undef s32
#undef s16
#undef s8
//typedef unsigned long long u64;
typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;
//typedef long long s64;
typedef long s32;
typedef short s16;
typedef char s8;

#define SZ_4K			(4 * 1024)

#define PWIDTH 720
#define PHIGH 576

#define STREAM_BUF_SIZE		0x200000
#define STREAM_FILL_SIZE	0x40000
#define STREAM_READ_SIZE	(512 * 8)
#define STREAM_END_SIZE		0
#define PS_SAVE_SIZE		0x080000
#define VP8_MB_SAVE_SIZE	0x080000
#define MPEG4_SCRATCH_SIZE	0x080000
#define MJPG_FILL_SIZE		(8 * 1024)

#define STREAM_ENC_PIC_RESET 	1

#define PATH_V4L2	0
#define PATH_FILE	1
#define PATH_NET	2
#define PATH_IPU	3
#ifdef BUILD_FOR_ANDROID
#define PATH_G2D	4
#endif
#define PATH_MEM  5

/* video format */
#define RAW_FMT 0
#define MKV_FMT 1
#define MP4_FMT 2

/* Test operations */
#define ENCODE		1
#define DECODE		2
#define LOOPBACK	3
#define TRANSCODE	4

/* ERROR CODE */
#define VR_OK                (0)
#define VR_ALLOC_ERR        (-1)
#define VR_CHECKDIR_ERR     (-2)
#define VR_CHDIR_ERR        (-3)
#define VR_MP4INIT_ERR      (-4)
#define VR_MP4TRACK_ERR     (-5)
#define VR_OPENIPU_ERR      (-6)
#define VR_INITIPU_ERR      (-7)


#define DEFAULT_PORT		5555
#define DEFAULT_PKT_SIZE	0x28000

#define SIZE_USER_BUF            0x1000
#define USER_DATA_INFO_OFFSET    8*17

/* Directory and file */
#define WORK_DIR "/media/mmcblk2p1/video"
//#define WORK_DIR "/home/root/video"
#define SUBDIR "camera"
#define DIR_FLAG 0777

/* Device */
#define VIDEO_DEVICE "/dev/video" 
#define IMG_DEVICE "/dev/mxc_ipu" 

/* Configure ID */
#define CONFIGFILE_PATH "/etc/vrecord.conf"

#define DEVICE_CHANNEL    "channel_num"
#define SAVE_PATH         "save_path"
#define VIDEO_DURATION    "video_duration"
#define VIDEO_NUM         "video_num"

extern int vrecord_dbg_level ;

#define dprintf(level, fmt, arg...)     if (vrecord_dbg_level >= level) \
        printf("[DEBUG]\t%s:%d " fmt, __FILE__, __LINE__, ## arg)

#define err_msg(fmt, arg...) do { if (vrecord_dbg_level >= 1)		\
	printf("[ERR]\t%s:%d " fmt,  __FILE__, __LINE__, ## arg); else \
	printf("[ERR]\t" fmt, ## arg);	\
	} while (0)
	
#define info_msg(fmt, arg...) do { if (vrecord_dbg_level >= 1)		\
	printf("[INFO]\t%s:%d " fmt,  __FILE__, __LINE__, ## arg); else \
	printf("[INFO]\t" fmt, ## arg);	\
	} while (0)
	
#define warn_msg(fmt, arg...) do { if (vrecord_dbg_level >= 1)		\
	printf("[WARN]\t%s:%d " fmt,  __FILE__, __LINE__, ## arg); else \
	printf("[WARN]\t" fmt, ## arg);	\
	} while (0)

enum {
    MODE420 = 0,
    MODE422 = 1,
    MODE224 = 2,
    MODE444 = 3,
    MODE400 = 4
};

struct frame_buf {
	int addrY;
	int addrCb;
	int addrCr;
	int strideY;
	int strideC;
	int mvColBuf;
	vpu_mem_desc desc;
};

struct v4l_buf {
	void *start;
	off_t offset;
	size_t length;
};

#define MAX_BUF_NUM	32
#define QUEUE_SIZE	(MAX_BUF_NUM + 1)
struct v4l_specific_data {
	struct v4l2_buffer buf;
	struct v4l_buf *buffers[MAX_BUF_NUM];
};

#ifdef BUILD_FOR_ANDROID
struct g2d_specific_data {
	struct g2d_buf *g2d_bufs[MAX_BUF_NUM];
};
#endif

struct buf_queue {
	int list[MAX_BUF_NUM + 1];
	int head;
	int tail;
};

struct ipu_buf {
	int ipu_paddr;
	void * ipu_vaddr;
	int field;
};


struct capture_testbuffer {
	size_t offset;
	unsigned int length;
};

struct rot {
	int rot_en;
	int ext_rot_en;
	int rot_angle;
};

#define MAX_PATH	256
struct vc_config {
	char input[MAX_PATH];	/* Input file name */
	char output[MAX_PATH];  /* Output file name */
	int src_scheme;
	int dst_scheme;
	int video_node;
	int video_node_capture;
	int src_fd;
	int dst_fd;
	int width;
	int height;
	int enc_width;
	int enc_height;
	int loff;
	int toff;
	int format;
	int deblock_en;
	int dering_en;
	int rot_en; /* Use VPU to do rotation */
	int ext_rot_en; /* Use IPU/GPU to do rotation */
	int rot_angle;
	int mirror;
	int chromaInterleave;
	int bitrate;
	int gop;
	int save_enc_hdr;
	int count;
	int prescan;
	int bs_mode;
	char *nbuf; /* network buffer */
	int nlen; /* remaining data in network buffer */
	int noffset; /* offset into network buffer */
	int seq_no; /* seq numbering to detect skipped frames */
	u16 port; /* udp port number */
	u16 complete; /* wait for the requested buf to be filled completely */
	int iframe;
	int mp4_h264Class;
	char vdi_motion;	/* VDI motion algorithm */
	int fps;
	int mapType;
	int quantParam;
    int channel;
    int video_fmt;
    int video_dru;
    int video_num;
    char *fpath;
};

struct encode {
	EncHandle handle;		/* Encoder handle */
	PhysicalAddress phy_bsbuf_addr; /* Physical bitstream buffer */
	u32 virt_bsbuf_addr;		/* Virtual bitstream buffer */
	int enc_picwidth;	/* Encoded Picture width */
	int enc_picheight;	/* Encoded Picture height */
	int src_picwidth;        /* Source Picture width */
	int src_picheight;       /* Source Picture height */
	int totalfb;	/* Total number of framebuffers allocated */
	int src_fbid;	/* Index of frame buffer that contains YUV image */
	FrameBuffer *fb; /* frame buffer base given to encoder */
	struct frame_buf **pfbpool; /* allocated fb pointers are stored here */
	ExtBufCfg scratchBuf;
	int mp4_dataPartitionEnable;
	int ringBufferEnable;
	int mjpg_fmt;
	int mvc_paraset_refresh_en;
	int mvc_extension;
	int linear2TiledEnable;
	int minFrameBufferCount;
	int avc_vui_present_flag;

    EncReportInfo mbInfo;
    EncReportInfo mvInfo;
    EncReportInfo sliceInfo;

	struct vc_config *config; /* command line */
    void * vptr;
	void * yuv_buff;
	u8 * huffTable;
	u8 * qMatTable;

	struct frame_buf fbpool[MAX_BUF_NUM];
};

/* mp4 format */
struct mp4_mux
{
    MP4FileHandle mp4file;
    MP4TrackId    video_track;
};

#if 0
/* mkv format */
struct mux_object_s
{
	mk_Track  * track;
    mk_TrackConfig *config;
    mk_Writer * writer;
};

typedef struct mux_object_s mux_object_t;
#endif
struct buffer {
        void* start;
        size_t length;
};

struct ipu_device
{
    int ifd;
    void *inbuf;
    void *outbuf;
    struct ipu_task *t;
};

struct video_device
{
    int channel;
    int vfd;

    struct buffer* buffers;
    unsigned int n_buffers;
};

struct video_record
{
    struct encode  enc;
    struct vc_config *config;
    struct video_device vdev;
    struct ipu_device idev;
    struct mp4_mux mp4_mux;
    int (*saveframe)(struct video_record *vrecord, char *buf, int n, int syncframe);
};

#endif

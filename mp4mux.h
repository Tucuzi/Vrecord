#ifndef _MP4MUX_H_
#define _MP4MUX_H_

#ifdef __cplusplus
extern "C" {
#endif

int mp4mux_init(struct video_record *vrecord, char * vfile_name);
int mp4_save_frame(struct video_record *vrecord, char *buf, int n, int syncframe);

#ifdef __cplusplus
}
#endif

#endif

#ifndef _CAPTURE_H_
#define _CAPTURE_H_

#ifdef __cplusplus
extern "C" {
#endif

int v4l_start_capturing(struct video_record *vrecord);
void v4l_stop_capturing(struct video_record *vrecord);
int v4l_capture_setup(struct video_record *vrecord);
int v4l_get_capture_data(struct video_record *vrecord, u8 *dbuf);
void v4l_put_capture_data(struct video_record *vrecord);
void v4l_close(struct video_record *vrecord);
#if __cplusplus
}
#endif

#endif

#ifndef _FB_H_
#define _FB_H_

#ifdef __cplusplus
extern "C" {
#endif

struct frame_buf *framebuf_alloc(struct frame_buf *fb, int stdMode, int format
, int strideY, int height, int mvCol);
int tiled_framebuf_base(FrameBuffer *fb, Uint32 frame_base, int strideY, int 
height, int mapType);
struct frame_buf *tiled_framebuf_alloc(struct frame_buf *fb, int stdMode, int 
format, int strideY, int height, int mvCol, int mapType);
void framebuf_free(struct frame_buf *fb);

#if __cplusplus
}
#endif

#endif

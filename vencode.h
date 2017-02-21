#ifndef _VENCODE_H_
#define _VENCODE_H_

#if 1
#ifdef __cplusplus
extern "C" {
#endif
#endif

int encoder_open(struct encode *enc);
void encoder_close(struct encode *enc);
int encoder_configure(struct encode *enc);
int encoder_setup(struct encode *enc);
int encoder_start(struct encode *enc);
int encoder_allocate_framebuffer(struct encode *enc);
void encoder_free_framebuffer(struct encode *enc);
#if 1
#ifdef __cplusplus
}
#endif
#endif
#endif

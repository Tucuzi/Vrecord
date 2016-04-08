#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>
#include "vrecord.hpp"

#define BUFF_NUM 8
#define CLEAR(x) memset (&(x), 0, sizeof (x))

int v4l_start_capturing(struct video_record *vrecord)
{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(vrecord->vdev.vfd, VIDIOC_STREAMON, &type) < 0) {
		err_msg("VIDIOC_STREAMON error\n");
		return -1;
	}

	return 0;
}

void v4l_stop_capturing(struct video_record *vrecord)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(vrecord->vdev.vfd, VIDIOC_STREAMOFF, &type);
}

void v4l_close(struct video_record *vrecord)
{
    int i;
    
    for (i = 0; i < vrecord->vdev.n_buffers; ++i)
        if (-1 == munmap(vrecord->vdev.buffers[i].start, vrecord->vdev.buffers[i].length))
            err_msg ("munmap error");

    close(vrecord->vdev.vfd);
	vrecord->vdev.vfd = -1;
}

int v4l_capture_setup(struct video_record *vrecord)
{
    int i;
    char video_dev[32];
    unsigned long file_length;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_capability cap;

    sprintf(video_dev, "%s%d", VIDEO_DEVICE, vrecord->vdev.channel);
    vrecord->vdev.vfd = open(video_dev, O_RDWR | O_NONBLOCK, 0);

    if (vrecord->vdev.vfd <0) {
        err_msg("Open %s fail!\n", video_dev);
        return -1;
    }

    ioctl(vrecord->vdev.vfd, VIDIOC_QUERYCAP, &cap);

    CLEAR (fmt);
    fmt.type                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width         = vrecord->config->width;
    fmt.fmt.pix.height        = vrecord->config->height;
    fmt.fmt.pix.pixelformat   = V4L2_PIX_FMT_YUYV;//V4L2_PIX_FMT_RGB565; //V4L2_PIX_FMT_YUV420;//V4L2_PIX_FMT_UYVY;
    fmt.fmt.pix.field         = V4L2_FIELD_INTERLACED;
    if (ioctl(vrecord->vdev.vfd, VIDIOC_S_FMT, &fmt) < 0) {
        err_msg("VIDIOC_S_FMT erro\n");
        return -1;
    }

    file_length = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;

    CLEAR (req);
    req.count   =  BUFF_NUM;
    req.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory  = V4L2_MEMORY_MMAP;

    if (ioctl(vrecord->vdev.vfd, VIDIOC_REQBUFS, &req) < 0) {
        err_msg("VIDIOC_REQBUFS erro\n");
        return -1;
    }
    
    if (req.count < BUFF_NUM)
        err_msg("Insufficient buffer memory\n");

    vrecord->vdev.buffers = calloc(req.count, sizeof (*vrecord->vdev.buffers));

    for (vrecord->vdev.n_buffers = 0; vrecord->vdev.n_buffers < req.count; vrecord->vdev.n_buffers++)
    {
        struct v4l2_buffer buf;    
        CLEAR (buf);
        buf.type          = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory        = V4L2_MEMORY_MMAP;
        buf.index         = vrecord->vdev.n_buffers;

        if (-1 == ioctl(vrecord->vdev.vfd, VIDIOC_QUERYBUF, &buf)) 
            err_msg ("VIDIOC_QUERYBUF error\n");

        vrecord->vdev.buffers[vrecord->vdev.n_buffers].length = buf.length;
        vrecord->vdev.buffers[vrecord->vdev.n_buffers].start =
            mmap (NULL /* start anywhere */,    
                buf.length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                vrecord->vdev.vfd, buf.m.offset);

        if (MAP_FAILED == vrecord->vdev.buffers[vrecord->vdev.n_buffers].start)
            err_msg ("mmap failed\n");
    }

    for (i = 0; i < vrecord->vdev.n_buffers; i++) {
        struct v4l2_buffer buf;
        CLEAR (buf);

        buf.type          = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory        = V4L2_MEMORY_MMAP;
        buf.index         = i;
        
        if (-1 == ioctl(vrecord->vdev.vfd, VIDIOC_QBUF, &buf))
            err_msg ("VIDIOC_QBUF failed\n");
    }

	return 0;
}

int v4l_get_capture_data(struct video_record *vrecord, u8 *dbuf)
{
    struct v4l2_buffer buf;
    fd_set fds;
    struct timeval tv;
    int r;

    FD_ZERO (&fds);
    FD_SET (vrecord->vdev.vfd, &fds);

    /* Timeout. */
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    r = select(vrecord->vdev.vfd + 1, &fds, NULL, NULL, &tv);

    if (-1 == r) {
        if (EINTR == errno)
            return -1;

        err_msg ("select err\n");
    }

    if (0 == r) {
        err_msg ("select timeout\n");
        exit (EXIT_FAILURE);
    }

    CLEAR (buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if(ioctl(vrecord->vdev.vfd, VIDIOC_DQBUF, &buf) < 0) {
		err_msg("VIDIOC_DQBUF failed\n");
		return -1;
	} 

    //assert (buf.index < vrecord->vdev.n_buffers);
    memcpy(dbuf, vrecord->vdev.buffers[buf.index].start, vrecord->vdev.buffers[buf.index].length);
    if (ioctl(vrecord->vdev.vfd, VIDIOC_QBUF, &buf) < 0) {
		err_msg("VIDIOC_QBUF failed\n");
		return -1;
	} 

    return vrecord->vdev.buffers[buf.index].length;
}

void v4l_put_capture_data(struct video_record *vrecord)
{
    struct v4l2_buffer buf;

    CLEAR (buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
	ioctl(vrecord->vdev.vfd, VIDIOC_QBUF, &buf);
}


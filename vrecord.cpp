#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>
#include <linux/videodev2.h>
//#include <lpu.h>
#include <uapi/linux/ipu.h>

#include "vrecord.hpp"
#include "parse.h"
#include "utils.h"
#include "mp4mux.h"
#include "vencode.hpp"
#include "capture.h"
//#include "muxmkv.h"

int vrecord_dbg_level = 0;
int debug_fd;

#define FRAME_NUM 10*25

struct record_obj
{
    struct vc_config * config;
    int channel;
};

//int file_fd;

static unsigned int fmt_to_bpp(unsigned int pixelformat)
{
    unsigned int bpp;

    switch (pixelformat)
    {
        case IPU_PIX_FMT_RGB565:
        /*interleaved 422*/
        case IPU_PIX_FMT_YUYV:
        case IPU_PIX_FMT_UYVY:
        /*non-interleaved 422*/
        case IPU_PIX_FMT_YUV422P:
        case IPU_PIX_FMT_YVU422P:
            bpp = 16;
            break;
        case IPU_PIX_FMT_BGR24:
        case IPU_PIX_FMT_RGB24:
        case IPU_PIX_FMT_YUV444:
        case IPU_PIX_FMT_YUV444P:
            bpp = 24;
            break;
        case IPU_PIX_FMT_BGR32:
        case IPU_PIX_FMT_BGRA32:
        case IPU_PIX_FMT_RGB32:
        case IPU_PIX_FMT_RGBA32:
        case IPU_PIX_FMT_ABGR32:
            bpp = 32;
            break;
        /*non-interleaved 420*/
        case IPU_PIX_FMT_YUV420P:
        case IPU_PIX_FMT_YVU420P:
        case IPU_PIX_FMT_YUV420P2:
        case IPU_PIX_FMT_NV12:
        case IPU_PIX_FMT_TILED_NV12:
            bpp = 12;
            break;
        default:
            bpp = 8;
            break;
    }

    return bpp;
}

void config_ipu_task(struct ipu_task *task)
{
    task->timeout = 1000;
    //config the inout video data
    task->input.width = PWIDTH;
    task->input.height = PHIGH;
    task->input.format =  IPU_PIX_FMT_RGB565;
    
    //config the output video data
    task->output.width = PWIDTH;
    task->output.height = PHIGH;
    task->output.format = IPU_PIX_FMT_YUV420P;
}

static int convert_save_frame(struct video_record *vrecord)
{
    int ret;

    ret = v4l_get_capture_data(vrecord, (u8*)vrecord->idev.inbuf);
    //write(debug_fd, vrecord->idev.inbuf, ret);
    #if 0
    ret = ioctl(vrecord->idev.ifd, IPU_QUEUE_TASK, vrecord->idev.t);
    if (ret < 0) {
        err_msg("ioct IPU_QUEUE_TASK fail\n");
    }
    else {
        vrecord->enc.yuv_buff = vrecord->idev.outbuf;
        encoder_start(&vrecord->enc);
        //write(debug_fd, vrecord->idev.outbuf,  PWIDTH * PHIGH * 3 / 2); 
    }
    #else
        vrecord->enc.yuv_buff = vrecord->idev.inbuf;
        encoder_start(&vrecord->enc);
    #endif
    
    return ret;
}

void config_init(int argc,char ** argv, struct vc_config * config)
{
    config->src_scheme = PATH_MEM;
    config->dst_scheme = PATH_FILE;
    config->width = PWIDTH;
    config->height = PHIGH;
    config->enc_width = PWIDTH;
    config->enc_height = PHIGH;    
    config->format = 2;//h.264
    config->chromaInterleave = 0;
    config->quantParam = 20;
    config->fps = 25; //25 frame per second
    config->video_fmt = MP4_FMT;
    config->fpath = WORK_DIR;
    config->video_dru = 10;
    config->video_num = 10;

    if (argc > 1)
        config->channel = atoi(argv[1]);
}

void * record_thread(void *pt)
{
    char cnt;
    char *vfile_name;
    char vfile_path[256];
    unsigned int i, ret, isize, osize;
    vpu_mem_desc    mem_desc = {0};
    vpu_mem_desc scratch_mem_desc = {0};
    struct video_record *Vrecord;
    struct record_obj *obj = (struct record_obj *)pt;

    Vrecord = (struct video_record *)calloc(1, sizeof(struct video_record));
       if (Vrecord == NULL) {
        err_msg("Failed to allocate video_record structure\n");
        ret = VR_ALLOC_ERR;
        //return ret;
        return NULL;
    }

    Vrecord->saveframe = mp4_save_frame;
    Vrecord->config = obj->config;
    Vrecord->enc.config = obj->config;
    Vrecord->enc.vptr = Vrecord;
    Vrecord->vdev.channel = obj->channel;

#if 0
    /* check the sub directory */
    ret = check_and_make_subdir(WORK_DIR, SUBDIR, obj->channel);
    if (ret)
        return ret;

    /* create the video file name */
    sprintf(vfile_path, "%s/%s%d", WORK_DIR, SUBDIR, obj->channel);

    ret = chdir(vfile_path);
    if (ret)
        return ret;

    vfile_name = get_the_filename(vfile_path);
    mp4mux_init(Vrecord, vfile_name);
#endif
    #if 0
    mobject = MKVInit( vfile_name );
    
    if (mobject == NULL) {
        err_msg("MKVInit fail\n");
        return -1;
    }
    #endif

    Vrecord->idev.ifd = open(IMG_DEVICE, O_RDWR, 0);
    if (Vrecord->idev.ifd < 0)  {
        perror("Open ipu device fail!");
        return NULL;
        //return VR_OPENIPU_ERR;
    }

    /* setting the vpu device */
    ret = vpu_Init(NULL);
    if (ret) {
        err_msg("VPU Init Failure.\n");
        return NULL;
        //return VR_INITIPU_ERR;
    }

    #if 0
    err = vpu_GetVersionInfo(&ver);
    if (err) {
        err_msg("Cannot get version info, err:%d\n", err);
        vpu_UnInit();
        return -1;
    }

    info_msg("VPU firmware version: %d.%d.%d_r%d\n", ver.fw_major, ver.fw_minor,
                        ver.fw_release, ver.fw_code);
    info_msg("VPU library version: %d.%d.%d\n", ver.lib_major, ver.lib_minor,
                        ver.lib_release);
    #endif

    /* get physical contigous bit stream buffer */
    mem_desc.size = STREAM_BUF_SIZE;
    ret = IOGetPhyMem(&mem_desc);
    if (ret) {
        err_msg("Unable to obtain physical memory\n");
        goto err;
    }

    /* mmap that physical buffer */
    if (IOGetVirtMem(&mem_desc) == -1) {
        err_msg("Unable to map physical memory\n");
        ret = -1;
        goto err;
    }

    Vrecord->enc.mjpg_fmt = MODE422;
    Vrecord->enc.phy_bsbuf_addr = mem_desc.phy_addr;
    Vrecord->enc.virt_bsbuf_addr = mem_desc.virt_uaddr;
    Vrecord->enc.yuv_buff = Vrecord->idev.outbuf; 

    if (obj->config->mapType) {
        Vrecord->enc.linear2TiledEnable = 1;
        obj->config->chromaInterleave = 1; /* Must be CbCrInterleave for tiled */
        if (obj->config->format == STD_MJPG) {
            err_msg("MJPG encoder cannot support tiled format\n");
            ret = -1;
            goto err;
        }
    } else
        Vrecord->enc.linear2TiledEnable = 0;

#if 0
    /* open the encoder */
    ret = encoder_open(&Vrecord->enc);
    if (ret)
        goto err;

    /* configure the encoder */
    ret = encoder_configure(&Vrecord->enc);
    if (ret)
        goto err1;

    /* allocate memory for the frame buffers */
    ret = encoder_allocate_framebuffer(&Vrecord->enc);
    if (ret)
        goto err1;

    //encoder_setup(&Vrecord->enc);
#endif

    /* setting the IPU device */
    Vrecord->idev.t =  (struct ipu_task*) malloc(sizeof(struct ipu_task));
    memset(Vrecord->idev.t, 0, sizeof(struct ipu_task));
    config_ipu_task(Vrecord->idev.t);
    isize = Vrecord->idev.t->input.paddr =
            Vrecord->idev.t->input.width * Vrecord->idev.t->input.height
            * fmt_to_bpp(Vrecord->idev.t->input.format)/8;
    
    ret = ioctl(Vrecord->idev.ifd, IPU_ALLOC, &Vrecord->idev.t->input.paddr);
    Vrecord->idev.inbuf = mmap(0, isize, PROT_READ | PROT_WRITE,
        MAP_SHARED, Vrecord->idev.ifd, Vrecord->idev.t->input.paddr);

    osize = Vrecord->idev.t->output.paddr =
        Vrecord->idev.t->output.width * Vrecord->idev.t->output.height
        * fmt_to_bpp(Vrecord->idev.t->output.format)/8;
    
    ret = ioctl(Vrecord->idev.ifd, IPU_ALLOC, &Vrecord->idev.t->output.paddr);
    Vrecord->idev.outbuf = mmap(0, osize, PROT_READ | PROT_WRITE,
        MAP_SHARED, Vrecord->idev.ifd, Vrecord->idev.t->output.paddr);

again:
    ret = ioctl(Vrecord->idev.ifd, IPU_CHECK_TASK, Vrecord->idev.t);
    if (ret != IPU_CHECK_OK) {
        if (ret > IPU_CHECK_ERR_MIN) {
            if (ret == IPU_CHECK_ERR_SPLIT_INPUTW_OVER) {
                Vrecord->idev.t->input.crop.w -= 8;
                goto again;
            }
            if (ret == IPU_CHECK_ERR_SPLIT_INPUTH_OVER) {
                Vrecord->idev.t->input.crop.h -= 8;
                goto again;
            }
            if (ret == IPU_CHECK_ERR_SPLIT_OUTPUTW_OVER) {
                Vrecord->idev.t->output.crop.w -= 8;
                goto again;
            }
            if (ret == IPU_CHECK_ERR_SPLIT_OUTPUTH_OVER) {
                Vrecord->idev.t->output.crop.h -= 8;
                goto again;
            }

            ret = 0;
            err_msg("ipu task check fail\n");
            //return -1;
            return NULL;
        }
    }

    /* setting the Camera device */
    v4l_capture_setup(Vrecord);
    v4l_start_capturing(Vrecord);

    /* check the sub directory */
    ret = check_and_make_subdir(Vrecord->config->fpath, SUBDIR, obj->channel);
    if (ret)
        goto finish;

    /* create the video file name */
    sprintf(vfile_path, "%s/%s%d", Vrecord->config->fpath, SUBDIR, obj->channel);
#if 0
    ret = chdir(vfile_path);
    if (ret)
        goto finish;
#endif
    cnt = 0;
    while(cnt++ < Vrecord->config->video_num || Vrecord->config->video_num == -1) {
        vfile_name = get_the_filename(vfile_path);
        info_msg("Create %s\n", vfile_name);

        /* open the encoder */
        ret = encoder_open(&Vrecord->enc);
            if (ret)
        goto err;

        /* configure the encoder */
        ret = encoder_configure(&Vrecord->enc);
        if (ret)
            goto err1;

        /* allocate memory for the frame buffers */
        ret = encoder_allocate_framebuffer(&Vrecord->enc);
        if (ret)
            goto err1;

        encoder_setup(&Vrecord->enc);
        
#if 0
        debug_fd = open(vfile_name, O_CREAT | O_RDWR | O_TRUNC,
                    S_IRWXU | S_IRWXG | S_IRWXO);
#else
        mp4mux_init(Vrecord, vfile_name);
#endif
        for (i = 0; i < Vrecord->config->video_dru * 25; i++)
        {
            convert_save_frame(Vrecord);
        }

        info_msg("Saved frame %d\n", i);
#if 0
        close(debug_fd);
#else
        MP4Close(Vrecord->mp4_mux.mp4file, 0);
#endif

        /* free the allocated framebuffers */
        encoder_free_framebuffer(&Vrecord->enc);
        /* close the encoder */
        encoder_close(&Vrecord->enc);
    }

    //MKVEnd(mobject);
finish:
    v4l_stop_capturing(Vrecord);
    goto err;

    /* free the allocated framebuffers */
    encoder_free_framebuffer(&Vrecord->enc);
err1:
    /* close the encoder */
    encoder_close(&Vrecord->enc);
err:
    if (cpu_is_mx6x() && obj->config->format == STD_MPEG4 && &Vrecord->enc.mp4_dataPartitionEnable) {
        IOFreeVirtMem(&scratch_mem_desc);
        IOFreePhyMem(&scratch_mem_desc);
    }

    /* free the physical memory */
    IOFreeVirtMem(&mem_desc);
    IOFreePhyMem(&mem_desc);
    vpu_UnInit();

    if (Vrecord->idev.outbuf)
        munmap(Vrecord->idev.outbuf, osize);

    if (Vrecord->idev.t->output.paddr)
        ioctl(Vrecord->idev.ifd, IPU_FREE, &Vrecord->idev.t->output.paddr);

    if (Vrecord->idev.inbuf)
        munmap(Vrecord->idev.inbuf, isize);

    if (Vrecord->idev.t->input.paddr)
        ioctl(Vrecord->idev.ifd, IPU_FREE, &Vrecord->idev.t->input.paddr);        

    close(Vrecord->idev.ifd);
    v4l_close(Vrecord);
    free(Vrecord);
    pthread_exit(NULL); 
}


int main(int argc,char ** argv)
{
    int ret;
    int channel;
    pthread_t tpid;
    pthread_attr_t pattr;
    struct vc_config * vcconfig;
    struct record_obj obj;
    struct parmeter_list *list;

    list = parse_config_file(CONFIGFILE_PATH);
    if (list == NULL){
        err_msg("Please check the config file %s\n", CONFIGFILE_PATH);
        return -1;
    }

    if (argc < 2) {
        err_msg("please input the camera channel id\n");
        return 0;
    }
    else 
        channel = atoi(argv[1]);

    if (channel < 0 && channel > 1 ) {
        err_msg("channel id %d is out of the correct arrange!\n", channel);
        return -1;
    }
    else
        info_msg("start recording the Camera%d\n", channel);

    vcconfig = (struct vc_config *)calloc(1, sizeof(struct vc_config));
    if (vcconfig == NULL) {
        err_msg("Failed to allocate vc_config structure\n");
        ret = VR_ALLOC_ERR;
        return ret;
    }

    config_init(argc, argv, vcconfig);
    vcconfig->video_dru = atoi(search_parmeter_value(list, VIDEO_DURATION));
    vcconfig->video_num = atoi(search_parmeter_value(list, VIDEO_NUM));
    vcconfig->fpath = search_parmeter_value(list, SAVE_PATH);

#if 0
    /* check the work directory */
    ret = check_and_make_workdir(vcconfig->fpath);
    if (ret)
        return ret;
#endif

    obj.channel = channel;
    obj.config = vcconfig;

    pthread_attr_init(&pattr);  
    pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE); 
    pthread_create(&tpid, &pattr, record_thread, (void*)&obj);

    pthread_join(tpid,NULL);

    info_msg("Job finish\n");
    sleep(1);
    return EXIT_SUCCESS;
}



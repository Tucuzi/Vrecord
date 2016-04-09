/*
 * Copyright 2004-2014 Freescale Semiconductor, Inc.
 *
 * Copyright (c) 2006, Chips & Media.  All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vrecord.hpp"
#include "utils.h"
#include "fb.h"
#include "vpu_jpegtable.hpp"

/* V4L2 capture buffers are obtained from here */
extern struct capture_testbuffer cap_buffers[];

/* When app need to exit */
extern int quitflag;
static int issyncframe;

void jpgGetHuffTable(EncMjpgParam *param)
{
    /* Rearrange and insert pre-defined Huffman table to deticated variable. */
    memcpy(param->huffBits[DC_TABLE_INDEX0], lumaDcBits, 16);   /* Luma DC BitLength */
    memcpy(param->huffVal[DC_TABLE_INDEX0], lumaDcValue, 16);   /* Luma DC HuffValue */

    memcpy(param->huffBits[AC_TABLE_INDEX0], lumaAcBits, 16);   /* Luma DC BitLength */
    memcpy(param->huffVal[AC_TABLE_INDEX0], lumaAcValue, 162);  /* Luma DC HuffValue */

    memcpy(param->huffBits[DC_TABLE_INDEX1], chromaDcBits, 16); /* Chroma DC BitLength */
    memcpy(param->huffVal[DC_TABLE_INDEX1], chromaDcValue, 16); /* Chroma DC HuffValue */

    memcpy(param->huffBits[AC_TABLE_INDEX1], chromaAcBits, 16); /* Chroma AC BitLength */
    memcpy(param->huffVal[AC_TABLE_INDEX1], chromaAcValue, 162); /* Chorma AC HuffValue */
}

void jpgGetQMatrix(EncMjpgParam *param)
{
    /* Rearrange and insert pre-defined Q-matrix to deticated variable. */
    memcpy(param->qMatTab[DC_TABLE_INDEX0], lumaQ2, 64);
    memcpy(param->qMatTab[AC_TABLE_INDEX0], chromaBQ2, 64);

    memcpy(param->qMatTab[DC_TABLE_INDEX1], param->qMatTab[DC_TABLE_INDEX0], 64);
    memcpy(param->qMatTab[AC_TABLE_INDEX1], param->qMatTab[AC_TABLE_INDEX0], 64);
}

void jpgGetCInfoTable(EncMjpgParam *param)
{
    int format = param->mjpg_sourceFormat;

    memcpy(param->cInfoTab, cInfoTable[format], 6 * 4);
}

static int
enc_readbs_reset_buffer(struct encode *enc, PhysicalAddress paBsBufAddr, int bsBufsize)
{
    u32 vbuf;
    struct video_record *vrecord = (struct video_record *)enc->vptr;
    
    vbuf = enc->virt_bsbuf_addr + paBsBufAddr - enc->phy_bsbuf_addr;
    vrecord->enc.output_ptr = (char*)vbuf;
    vrecord->enc.outlen = bsBufsize;

    return vrecord->saveframe(vrecord, (char *)vbuf, bsBufsize, issyncframe);
    //return vpu_write(enc->config, (void *)vbuf, bsBufsize);
}

static int
enc_readbs_ring_buffer(EncHandle handle, struct vc_config *config,
        u32 bs_va_startaddr, u32 bs_va_endaddr, u32 bs_pa_startaddr,
        int defaultsize)
{
    RetCode ret;
    int space = 0, room;
    PhysicalAddress pa_read_ptr, pa_write_ptr;
    u32 target_addr, size;

    info_msg("enc_readbs_ring_buffer\n");
    ret = vpu_EncGetBitstreamBuffer(handle, &pa_read_ptr, &pa_write_ptr,
                    (Uint32 *)&size);
    if (ret != RETCODE_SUCCESS) {
        err_msg("EncGetBitstreamBuffer failed\n");
        return -1;
    }

    /* No space in ring buffer */
    if (size <= 0)
        return 0;

    if (defaultsize > 0) {
        if (size < defaultsize)
            return 0;

        space = defaultsize;
    } else {
        space = size;
    }

    if (space > 0) {
        target_addr = bs_va_startaddr + (pa_read_ptr - bs_pa_startaddr);
        if ( (target_addr + space) > bs_va_endaddr) {
            room = bs_va_endaddr - target_addr;
            vpu_write(config, (char *)target_addr, room);
            vpu_write(config, (char *)bs_va_startaddr,(space - room));
        } else {
            vpu_write(config, (char *)target_addr, space);
        }

        ret = vpu_EncUpdateBitstreamBuffer(handle, space);
        if (ret != RETCODE_SUCCESS) {
            err_msg("EncUpdateBitstreamBuffer failed\n");
            return -1;
        }
    }

    return space;
}

void dump_header(struct encode *enc, PhysicalAddress paBsBufAddr, int size)
{
    int i;
    u32 vbuf;
    u8 *buf;

    vbuf = enc->virt_bsbuf_addr + paBsBufAddr - enc->phy_bsbuf_addr;
    buf = (u8 *)vbuf;
    for(i = 0; i < size; i++)
        printf("0x%x ", buf[i]);
        
    printf("\n");
}

static int
encoder_fill_headers(struct encode *enc)
{
    EncHeaderParam enchdr_param = {0};
    EncHandle handle = enc->handle;
    //RetCode ret = RETCODE_SUCCESS;
    int ret = 0;    
    int mbPicNum;

    /* Must put encode header before encoding */
    if (enc->config->format == STD_MPEG4) {
        enchdr_param.headerType = VOS_HEADER;

        if (cpu_is_mx6x())
            goto put_mp4header;
        /*
         * Please set userProfileLevelEnable to 0 if you need to generate
             * user profile and level automaticaly by resolution, here is one
         * sample of how to work when userProfileLevelEnable is 1.
         */
        enchdr_param.userProfileLevelEnable = 1;
        mbPicNum = ((enc->enc_picwidth + 15) / 16) *((enc->enc_picheight + 15) / 16);
        if (enc->enc_picwidth <= 176 && enc->enc_picheight <= 144 &&
            mbPicNum * enc->config->fps <= 1485)
            enchdr_param.userProfileLevelIndication = 8; /* L1 */
        /* Please set userProfileLevelIndication to 8 if L0 is needed */
        else if (enc->enc_picwidth <= 352 && enc->enc_picheight <= 288 &&
             mbPicNum * enc->config->fps <= 5940)
            enchdr_param.userProfileLevelIndication = 2; /* L2 */
        else if (enc->enc_picwidth <= 352 && enc->enc_picheight <= 288 &&
             mbPicNum * enc->config->fps <= 11880)
            enchdr_param.userProfileLevelIndication = 3; /* L3 */
        else if (enc->enc_picwidth <= 640 && enc->enc_picheight <= 480 &&
             mbPicNum * enc->config->fps <= 36000)
            enchdr_param.userProfileLevelIndication = 4; /* L4a */
        else if (enc->enc_picwidth <= 720 && enc->enc_picheight <= 576 &&
             mbPicNum * enc->config->fps <= 40500)
            enchdr_param.userProfileLevelIndication = 5; /* L5 */
        else
            enchdr_param.userProfileLevelIndication = 6; /* L6 */

put_mp4header:
        vpu_EncGiveCommand(handle, ENC_PUT_MP4_HEADER, &enchdr_param);
        if (enc->ringBufferEnable == 0 ) {
            //ret = enc_readbs_reset_buffer(enc, enchdr_param.buf, enchdr_param.size);
            if (ret < 0)
                return -1;
        }

        enchdr_param.headerType = VIS_HEADER;
        vpu_EncGiveCommand(handle, ENC_PUT_MP4_HEADER, &enchdr_param);
        if (enc->ringBufferEnable == 0 ) {
            //ret = enc_readbs_reset_buffer(enc, enchdr_param.buf, enchdr_param.size);
            if (ret < 0)
                return -1;
        }

        enchdr_param.headerType = VOL_HEADER;
        vpu_EncGiveCommand(handle, ENC_PUT_MP4_HEADER, &enchdr_param);
        if (enc->ringBufferEnable == 0 ) {
            //ret = enc_readbs_reset_buffer(enc, enchdr_param.buf, enchdr_param.size);
            if (ret < 0)
                return -1;
        }
    } else if (enc->config->format == STD_AVC) {
        if (enc->mvc_extension && enc->mvc_paraset_refresh_en)
            goto skip_put_header;
        {
            if (!enc->avc_vui_present_flag) {
                enchdr_param.headerType = SPS_RBSP;
                vpu_EncGiveCommand(handle, ENC_PUT_AVC_HEADER, &enchdr_param);
                if (enc->ringBufferEnable == 0 ) {
                    //dump_header(enc, enchdr_param.buf, enchdr_param.size);
                    //info_msg("SPS_RBSP\n");
                    ret = enc_readbs_reset_buffer(enc, enchdr_param.buf, enchdr_param.size);
                    if (ret < 0)
                        return -1;
                }
            } else {
                /*
                 * Not support MVC
                 * Not support ring buffer mode
                 */
                unsigned char *pBuffer = (unsigned char *)malloc(STREAM_BUF_SIZE);

                if (pBuffer) {
                    enchdr_param.headerType = SPS_RBSP;
                    enchdr_param.pBuf = pBuffer;
                    enchdr_param.size = STREAM_BUF_SIZE;
                    ret = vpu_EncGiveCommand(handle, ENC_GET_VIDEO_HEADER, &enchdr_param);
                    if (ret == RETCODE_SUCCESS) {
                        //info_msg("mvc SPS_RBSP\n");
                        vpu_write(enc->config, (char *)pBuffer, enchdr_param.size);
                        free(pBuffer);
                    } else {
                        err_msg("ENC_GET_VIDEO_HEADER failure\n");
                        free(pBuffer);
                        return -1;
                    }
                } else {
                    err_msg("memory allocate failure\n");
                    return -1;
                }
            }
        }

        if (enc->mvc_extension) {
            enchdr_param.headerType = SPS_RBSP_MVC;
            vpu_EncGiveCommand(handle, ENC_PUT_AVC_HEADER, &enchdr_param);
            if (enc->ringBufferEnable == 0 ) {
                //info_msg("SPS_RBSP_MVC\n");
                ret = enc_readbs_reset_buffer(enc, enchdr_param.buf, enchdr_param.size);
                if (ret < 0)
                    return -1;
            }
        }

        enchdr_param.headerType = PPS_RBSP;
        vpu_EncGiveCommand(handle, ENC_PUT_AVC_HEADER, &enchdr_param);
        if (enc->ringBufferEnable == 0 ) {
            //info_msg("PPS_RBSP\n");
            ret = enc_readbs_reset_buffer(enc, enchdr_param.buf, enchdr_param.size);
            if (ret < 0)
                return -1;
        }

        if (enc->mvc_extension) { /* MVC */
            enchdr_param.headerType = PPS_RBSP_MVC;
            vpu_EncGiveCommand(handle, ENC_PUT_AVC_HEADER, &enchdr_param);
            if (enc->ringBufferEnable == 0 ) {
                //info_msg("PPS_RBSP_MVC\n");
                //dump_header(enc, enchdr_param.buf, enchdr_param.size);
                ret = enc_readbs_reset_buffer(enc, enchdr_param.buf, enchdr_param.size);
                if (ret < 0)
                    return -1;
            }
        }
    } else if (enc->config->format == STD_MJPG) {
        if (enc->huffTable)
            free(enc->huffTable);
        if (enc->qMatTable)
            free(enc->qMatTable);
        if (cpu_is_mx6x()) {
            int enableSofStuffing = 1;
            EncParamSet enchdr_param = {0};
            vpu_EncGiveCommand(handle, ENC_ENABLE_SOF_STUFF, &enableSofStuffing);
            enchdr_param.size = STREAM_BUF_SIZE;
            enchdr_param.pParaSet = (Uint8 *)malloc(STREAM_BUF_SIZE);
            if (enchdr_param.pParaSet) {
                vpu_EncGiveCommand(handle,ENC_GET_JPEG_HEADER, &enchdr_param);
                vpu_write(enc->config, (char *)enchdr_param.pParaSet, enchdr_param.size);
                free(enchdr_param.pParaSet);
            } else {
                err_msg("memory allocate failure\n");
                return -1;
            }
        }
    }

skip_put_header:
    return 0;
}

void
encoder_free_framebuffer(struct encode *enc)
{
    int i;

    if (enc->pfbpool) {
        for (i = 0; i < enc->totalfb; i++) {
            framebuf_free(enc->pfbpool[i]);
        }
    }

    if (enc->fb) {
        free(enc->fb);
        enc->fb = NULL;
    }
    if (enc->pfbpool) {
        free(enc->pfbpool);
        enc->pfbpool = NULL;
    }
}

int
encoder_allocate_framebuffer(struct encode *enc)
{
    EncHandle handle = enc->handle;
    int i, enc_stride, src_stride, src_fbid;
    int totalfb, minfbcount, srcfbcount, extrafbcount;
    RetCode ret;
    FrameBuffer *fb;
    PhysicalAddress subSampBaseA = 0, subSampBaseB = 0;
    struct frame_buf **pfbpool;
    EncExtBufInfo extbufinfo = {0};
    int enc_fbwidth, enc_fbheight, src_fbwidth, src_fbheight;

    minfbcount = enc->minFrameBufferCount;
    dprintf(4, "minfb %d\n", minfbcount);
    srcfbcount = 1;

    enc_fbwidth = (enc->enc_picwidth + 15) & ~15;
    enc_fbheight = (enc->enc_picheight + 15) & ~15;
    src_fbwidth = (enc->src_picwidth + 15) & ~15;
    src_fbheight = (enc->src_picheight + 15) & ~15;

    if (cpu_is_mx6x()) {
        if (enc->config->format == STD_AVC && enc->mvc_extension) /* MVC */
            extrafbcount = 2 + 2; /* Subsamp [2] + Subsamp MVC [2] */
        else if (enc->config->format == STD_MJPG)
            extrafbcount = 0;
        else
            extrafbcount = 2; /* Subsamp buffer [2] */
    } else
        extrafbcount = 0;

    enc->totalfb = totalfb = minfbcount + extrafbcount + srcfbcount;

    /* last framebuffer is used as src frame in the test */
    enc->src_fbid = src_fbid = totalfb - 1;

    fb = enc->fb = (FrameBuffer *)calloc(totalfb, sizeof(FrameBuffer));
    if (fb == NULL) {
        err_msg("Failed to allocate enc->fb\n");
        return -1;
    }

    pfbpool = enc->pfbpool = (struct frame_buf **)calloc(totalfb,
                    sizeof(struct frame_buf *));
    if (pfbpool == NULL) {
        err_msg("Failed to allocate enc->pfbpool\n");
        free(enc->fb);
        enc->fb = NULL;
        return -1;
    }

    if (enc->config->mapType == LINEAR_FRAME_MAP) {
        /* All buffers are linear */
        for (i = 0; i < minfbcount + extrafbcount; i++) {
            pfbpool[i] = framebuf_alloc(&enc->fbpool[i], enc->config->format, enc->mjpg_fmt,
                            enc_fbwidth, enc_fbheight, 0);
            if (pfbpool[i] == NULL) {
                goto err1;
            }
        }
     } else {
        /* Encoded buffers are tiled */
        for (i = 0; i < minfbcount; i++) {
            pfbpool[i] = tiled_framebuf_alloc(&enc->fbpool[i], enc->config->format, enc->mjpg_fmt,
                        enc_fbwidth, enc_fbheight, 0, enc->config->mapType);
            if (pfbpool[i] == NULL)
                goto err1;
        }
        /* sub frames are linear */
        for (i = minfbcount; i < minfbcount + extrafbcount; i++) {
            pfbpool[i] = framebuf_alloc(&enc->fbpool[i], enc->config->format, enc->mjpg_fmt,
                            enc_fbwidth, enc_fbheight, 0);
            if (pfbpool[i] == NULL)
                goto err1;
        }
    }

    for (i = 0; i < minfbcount + extrafbcount; i++) {
        fb[i].myIndex = i;
        fb[i].bufY = pfbpool[i]->addrY;
        fb[i].bufCb = pfbpool[i]->addrCb;
        fb[i].bufCr = pfbpool[i]->addrCr;
        fb[i].strideY = pfbpool[i]->strideY;
        fb[i].strideC = pfbpool[i]->strideC;
    }

    if (cpu_is_mx6x() && (enc->config->format != STD_MJPG)) {
        subSampBaseA = fb[minfbcount].bufY;
        subSampBaseB = fb[minfbcount + 1].bufY;
        if (enc->config->format == STD_AVC && enc->mvc_extension) { /* MVC */
            extbufinfo.subSampBaseAMvc = fb[minfbcount + 2].bufY;
            extbufinfo.subSampBaseBMvc = fb[minfbcount + 3].bufY;
        }
    }

    /* Must be a multiple of 16 */
    if (enc->config->rot_angle == 90 || enc->config->rot_angle == 270)
        enc_stride = (enc->enc_picheight + 15 ) & ~15;
    else
        enc_stride = (enc->enc_picwidth + 15) & ~15;
    src_stride = (enc->src_picwidth + 15 ) & ~15;

    extbufinfo.scratchBuf = enc->scratchBuf;
    ret = vpu_EncRegisterFrameBuffer(handle, fb, minfbcount, enc_stride, src_stride,
                        subSampBaseA, subSampBaseB, &extbufinfo);
    if (ret != RETCODE_SUCCESS) {
        err_msg("Register frame buffer failed\n");
        goto err1;
    }


    {
        /* Allocate a single frame buffer for source frame */
        pfbpool[src_fbid] = framebuf_alloc(&enc->fbpool[src_fbid], enc->config->format, enc->mjpg_fmt,
                           src_fbwidth, src_fbheight, 0);
        if (pfbpool[src_fbid] == NULL) {
            err_msg("failed to allocate single framebuf\n");
            goto err1;
        }

        fb[src_fbid].myIndex = enc->src_fbid;
        fb[src_fbid].bufY = pfbpool[src_fbid]->addrY;
        fb[src_fbid].bufCb = pfbpool[src_fbid]->addrCb;
        fb[src_fbid].bufCr = pfbpool[src_fbid]->addrCr;
        fb[src_fbid].strideY = pfbpool[src_fbid]->strideY;
        fb[src_fbid].strideC = pfbpool[src_fbid]->strideC;
    }

    return 0;

err1:
    for (i = 0; i < totalfb; i++) {
        framebuf_free(pfbpool[i]);
    }

    free(enc->fb);
    free(enc->pfbpool);
    enc->fb = NULL;
    enc->pfbpool = NULL;
    return -1;
}

static int
read_from_ram(struct encode *enc)
{
    u32 y_addr, u_addr, v_addr;
    struct frame_buf *pfb = enc->pfbpool[enc->src_fbid];
    int divX, divY;
    int format = enc->mjpg_fmt;
    int chromaInterleave = enc->config->chromaInterleave;
    int img_size, y_size, c_size;
    int ret = 0;

    if (enc->src_picwidth != pfb->strideY) {
        err_msg("Make sure src pic width is a multiple of 16\n");
        return -1;
    }

    divX = (format == MODE420 || format == MODE422) ? 2 : 1;
    divY = (format == MODE420 || format == MODE224) ? 2 : 1;

    y_size = enc->src_picwidth * enc->src_picheight;
    c_size = y_size / divX / divY;
    img_size = y_size + c_size * 2;

    y_addr = pfb->addrY + pfb->desc.virt_uaddr - pfb->desc.phy_addr;


#if 1
    u_addr = pfb->addrCb + pfb->desc.virt_uaddr - pfb->desc.phy_addr;
    v_addr = pfb->addrCr + pfb->desc.virt_uaddr - pfb->desc.phy_addr;
#else
    v_addr = pfb->addrCb + pfb->desc.virt_uaddr - pfb->desc.phy_addr;
    u_addr = pfb->addrCr + pfb->desc.virt_uaddr - pfb->desc.phy_addr;
#endif

    if (img_size == pfb->desc.size) {
        memcpy((void *)y_addr, (void *)enc->yuv_buff, img_size);

    } else {
        memcpy((void *)y_addr, (void *)enc->yuv_buff, y_size);
        if (chromaInterleave == 0) {
            memcpy((void *)u_addr, (void *)enc->yuv_buff + y_size, c_size);
            memcpy((void *)v_addr, (void *)enc->yuv_buff + y_size + c_size, c_size);
        } else {
            memcpy((void *)u_addr, (void *)enc->yuv_buff + y_size, c_size * 2);
        }
    }

    return ret;
}

int 
encoder_setup(struct encode *enc)
{
    EncHandle handle = enc->handle;
    //RetCode ret = RETCODE_SUCCESS;
    int ret = 0;

    /* Must put encode header here before encoding for all codec, except MX6 MJPG */
    if (!(cpu_is_mx6x() && (enc->config->format == STD_MJPG))) {
        ret = encoder_fill_headers(enc);
        if (ret) {
            err_msg("Encode fill headers failed\n");
            return -1;
        }
    }
    
    /* Set report info flag */
    if (enc->mbInfo.enable) {
        ret = vpu_EncGiveCommand(handle, ENC_SET_REPORT_MBINFO, &enc->mbInfo);
        if (ret != RETCODE_SUCCESS) {
            err_msg("Failed to set MbInfo report, ret %d\n", ret);
            return -1;
        }
    }
    if (enc->mvInfo.enable) {
        ret = vpu_EncGiveCommand(handle, ENC_SET_REPORT_MVINFO, &enc->mvInfo);
        if (ret != RETCODE_SUCCESS) {
            err_msg("Failed to set MvInfo report, ret %d\n", ret);
            return -1;
        }
    }
    if (enc->sliceInfo.enable) {
        ret = vpu_EncGiveCommand(handle, ENC_SET_REPORT_SLICEINFO, &enc->sliceInfo);
        if (ret != RETCODE_SUCCESS) {
            err_msg("Failed to set slice info report, ret %d\n", ret);
            return -1;
        }
    }

    return 0;
}

int
encoder_start(struct encode *enc)
{
    EncHandle handle = enc->handle;
    EncParam  enc_param = {0};
    EncOutputInfo outinfo = {0};
    //RetCode ret = RETCODE_SUCCESS;
    int ret = 0;    
    int src_fbid = enc->src_fbid;
    int img_size, frame_id = 0;
    //struct timeval tenc_begin,tenc_end, total_start, total_end;
    //int sec, usec; 
    int loop_id;
    //float tenc_time = 0, total_time=0;
    PhysicalAddress phy_bsbuf_start = enc->phy_bsbuf_addr;
    u32 virt_bsbuf_start = enc->virt_bsbuf_addr;
    u32 virt_bsbuf_end = virt_bsbuf_start + STREAM_BUF_SIZE;
    int is_waited_int = 0;

    enc_param.sourceFrame = &enc->fb[src_fbid];
    enc_param.quantParam = 23;
    enc_param.forceIPicture = 0;
    enc_param.skipPicture = 0;
    enc_param.enableAutoSkip = 1;

    enc_param.encLeftOffset = 0;
    enc_param.encTopOffset = 0;
    if ((enc_param.encLeftOffset + enc->enc_picwidth) > enc->src_picwidth) {
        err_msg("Configure is failure for width and left offset\n");
        return -1;
    }

    if ((enc_param.encTopOffset + enc->enc_picheight) > enc->src_picheight) {
        err_msg("Configure is failure for height and top offset\n");
        return -1;
    }

    img_size = enc->src_picwidth * enc->src_picheight * 3 / 2;
    if (enc->config->format == STD_MJPG) {
        if (enc->mjpg_fmt == MODE422 || enc->mjpg_fmt == MODE224)
            img_size = enc->src_picwidth * enc->src_picheight * 2;
        else if (enc->mjpg_fmt == MODE400)
            img_size = enc->src_picwidth * enc->src_picheight;
    }

    //gettimeofday(&total_start, NULL);

    /* The main encoding processing */
    ret = read_from_ram(enc);
    //if (ret <= 0)
        //return -1;

    /* Must put encode header before each frame encoding for mx6 MJPG */
    if (cpu_is_mx6x() && (enc->config->format == STD_MJPG)) {
        info_msg("fill header\n");
        ret = encoder_fill_headers(enc);
        if (ret) {
            err_msg("Encode fill headers failed\n");
            goto err2;
        }
    }

//        gettimeofday(&tenc_begin, NULL);
    ret = vpu_EncStartOneFrame(handle, &enc_param);
    if (ret != RETCODE_SUCCESS) {
        err_msg("vpu_EncStartOneFrame failed Err code:%d\n", ret);
        goto err2;
    }

    is_waited_int = 0;
    loop_id = 0;
    while (vpu_IsBusy()) {
        if (enc->ringBufferEnable == 1) {
            if (cpu_is_mx6x() && enc->config->format == STD_MJPG) {
                ret = enc_readbs_ring_buffer(handle, enc->config,
                        virt_bsbuf_start, virt_bsbuf_end,
                        phy_bsbuf_start, 0);
            }
            else {
                ret = enc_readbs_ring_buffer(handle, enc->config,
                        virt_bsbuf_start, virt_bsbuf_end,
                        phy_bsbuf_start, STREAM_READ_SIZE);
            }

            if (ret < 0) {
                goto err2;
            }
        }
        if (loop_id == 20) {
            ret = vpu_SWReset(handle, 0);
            return -1;
        }
        if (vpu_WaitForInt(200) == 0)
            is_waited_int = 1;
        loop_id ++;
    }

    if (!is_waited_int)
        vpu_WaitForInt(200);

#if 0
        gettimeofday(&tenc_end, NULL);
        sec = tenc_end.tv_sec - tenc_begin.tv_sec;
        usec = tenc_end.tv_usec - tenc_begin.tv_usec;

        if (usec < 0) {
            sec--;
            usec = usec + 1000000;
        }

        tenc_time += (sec * 1000000) + usec;
#endif

    ret = vpu_EncGetOutputInfo(handle, &outinfo);
    if(outinfo.picType == 0) 
        issyncframe = 1;
    else
        issyncframe = 0;
        
    usleep(0);

    dprintf(3, "frame_id %d\n", (int)frame_id);

    if (ret != RETCODE_SUCCESS) {
        err_msg("vpu_EncGetOutputInfo failed Err code: %d\n", ret);
        goto err2;
    }

    if (outinfo.skipEncoded)
        info_msg("Skip encoding one Frame!\n");

    if (enc->ringBufferEnable == 0) {
        ret = enc_readbs_reset_buffer(enc, outinfo.bitstreamBuffer, outinfo.bitstreamSize);
        if (ret < 0) {
            err_msg("writing bitstream buffer failed\n");
            goto err2;
        }
    } else {
        enc_readbs_ring_buffer(handle, enc->config, virt_bsbuf_start,
                    virt_bsbuf_end, phy_bsbuf_start, 0);
    }

    frame_id++;
    
# if 0
    gettimeofday(&total_end, NULL);
    sec = total_end.tv_sec - total_start.tv_sec;
    usec = total_end.tv_usec - total_start.tv_usec;
    if (usec < 0) {
        sec--;
        usec = usec + 1000000;
    }
    total_time = (sec * 1000000) + usec;

    info_msg("Finished encoding: %d frames\n", frame_id);
    info_msg("enc fps = %.2f\n", (frame_id / (tenc_time / 1000000)));
    info_msg("total fps= %.2f \n",(frame_id / (total_time / 1000000)));
#endif

err2:
    /* Inform the other end that no more frames will be sent */
    if (enc->config->dst_scheme == PATH_NET) {
        vpu_write(enc->config, NULL, 0);
    }

    /* For automation of test case */
    if (ret > 0)
        ret = 0;

    return ret;
}

int
encoder_configure(struct encode *enc)
{
    EncHandle handle = enc->handle;
    SearchRamParam search_pa = {0};
    EncInitialInfo initinfo = {0};
    RetCode ret;
    MirrorDirection mirror;
    int intraRefreshMode = 1;

    if (cpu_is_mx27()) {
        search_pa.searchRamAddr = 0xFFFF4C00;
        ret = vpu_EncGiveCommand(handle, ENC_SET_SEARCHRAM_PARAM, &search_pa);
        if (ret != RETCODE_SUCCESS) {
            err_msg("Encoder SET_SEARCHRAM_PARAM failed\n");
            return -1;
        }
    }
    
    if (enc->config->rot_en) {
        vpu_EncGiveCommand(handle, ENABLE_ROTATION, 0);
        vpu_EncGiveCommand(handle, ENABLE_MIRRORING, 0);
        vpu_EncGiveCommand(handle, SET_ROTATION_ANGLE,
                    &enc->config->rot_angle);
        mirror = (MirrorDirection)enc->config->mirror;
        vpu_EncGiveCommand(handle, SET_MIRROR_DIRECTION, &mirror);
    }

    vpu_EncGiveCommand(handle, ENC_SET_INTRA_REFRESH_MODE, &intraRefreshMode);

    ret = vpu_EncGetInitialInfo(handle, &initinfo);
    if (ret != RETCODE_SUCCESS) {
        err_msg("Encoder GetInitialInfo failed\n");
        return -1;
    }

    enc->minFrameBufferCount = initinfo.minFrameBufferCount;
    if (enc->config->save_enc_hdr) {
        if (enc->config->format == STD_MPEG4) {
            SaveGetEncodeHeader(handle, ENC_GET_VOS_HEADER,
                        "mp4_vos_header.dat");
            SaveGetEncodeHeader(handle, ENC_GET_VO_HEADER,
                        "mp4_vo_header.dat");
            SaveGetEncodeHeader(handle, ENC_GET_VOL_HEADER,
                        "mp4_vol_header.dat");
        } else if (enc->config->format == STD_AVC) {
            SaveGetEncodeHeader(handle, ENC_GET_SPS_RBSP,
                        "avc_sps_header.dat");
            SaveGetEncodeHeader(handle, ENC_GET_PPS_RBSP,
                        "avc_pps_header.dat");
        }
    }

    enc->mbInfo.enable = 0;
    enc->mvInfo.enable = 0;
    enc->sliceInfo.enable = 0;

    if (enc->mbInfo.enable) {
        enc->mbInfo.addr = (Uint8*)malloc(initinfo.reportBufSize.mbInfoBufSize);
        if (!enc->mbInfo.addr)
            err_msg("malloc_error\n");
    }
    if (enc->mvInfo.enable) {
        enc->mvInfo.addr = (Uint8*)malloc(initinfo.reportBufSize.mvInfoBufSize);
        if (!enc->mvInfo.addr)
            err_msg("malloc_error\n");
    }
    if (enc->sliceInfo.enable) {
        enc->sliceInfo.addr = (Uint8*)malloc(initinfo.reportBufSize.sliceInfoBufSize);
        if (!enc->sliceInfo.addr)
            err_msg("malloc_error\n");
    }

    return 0;
}

void
encoder_close(struct encode *enc)
{
    RetCode ret;

       if (enc->mbInfo.addr)
        free(enc->mbInfo.addr);
    if (enc->mbInfo.addr)
        free(enc->mbInfo.addr);
    if (enc->sliceInfo.addr)
        free(enc->sliceInfo.addr);

    ret = vpu_EncClose(enc->handle);
    if (ret == RETCODE_FRAME_NOT_COMPLETE) {
        vpu_SWReset(enc->handle, 0);
        vpu_EncClose(enc->handle);
    }
}

int
encoder_open(struct encode *enc)
{
    EncHandle handle = {0};
    EncOpenParam encop = {0};
    Uint8 *huffTable = enc->huffTable;
    Uint8 *qMatTable = enc->qMatTable;
    int i;
    RetCode ret;

    /* Fill up parameters for encoding */
    encop.bitstreamBuffer = enc->phy_bsbuf_addr;
    encop.bitstreamBufferSize = STREAM_BUF_SIZE;
    encop.bitstreamFormat = (CodStd)enc->config->format;
    encop.mapType = enc->config->mapType;
    encop.linear2TiledEnable = enc->linear2TiledEnable;
    /* width and height in command line means source image size */
    if (enc->config->width && enc->config->height) {
        enc->src_picwidth = enc->config->width;
        enc->src_picheight = enc->config->height;
    }

    /* enc_width and enc_height in command line means encoder output size */
    if (enc->config->enc_width && enc->config->enc_height) {
        enc->enc_picwidth = enc->config->enc_width;
        enc->enc_picheight = enc->config->enc_height;
    } else {
        enc->enc_picwidth = enc->src_picwidth;
        enc->enc_picheight = enc->src_picheight;
    }

    /* If rotation angle is 90 or 270, pic width and height are swapped */
    if (enc->config->rot_angle == 90 || enc->config->rot_angle == 270) {
        encop.picWidth = enc->enc_picheight;
        encop.picHeight = enc->enc_picwidth;
    } else {
        encop.picWidth = enc->enc_picwidth;
        encop.picHeight = enc->enc_picheight;
    }

    if (enc->config->fps == 0)
        enc->config->fps = 30;

//    info_msg("Capture/Encode fps will be %d\n", enc->config->fps);

    /*Note: Frame rate cannot be less than 15fps per H.263 spec */
    encop.frameRateInfo = enc->config->fps;
    encop.bitRate = enc->config->bitrate;
    encop.gopSize = enc->config->gop;
    encop.slicemode.sliceMode = 0;    /* 0: 1 slice per picture; 1: Multiple slices per picture */
    encop.slicemode.sliceSizeMode = 0; /* 0: silceSize defined by bits; 1: sliceSize defined by MB number*/
    encop.slicemode.sliceSize = 4000;  /* Size of a slice in bits or MB numbers */

    encop.initialDelay = 0;
    encop.vbvBufferSize = 0;        /* 0 = ignore 8 */
    encop.intraRefresh = 0;
    encop.sliceReport = 0;
    encop.mbReport = 0;
    encop.mbQpReport = 0;
    encop.rcIntraQp = -1;
    encop.userQpMax = 0;
    encop.userQpMin = 0;
    encop.userQpMinEnable = 0;
    encop.userQpMaxEnable = 0;

    encop.IntraCostWeight = 0;
    encop.MEUseZeroPmv  = 0;
    /* (3: 16x16, 2:32x16, 1:64x32, 0:128x64, H.263(Short Header : always 3) */
    encop.MESearchRange = 3;

    encop.userGamma = (Uint32)(0.75*32768);         /*  (0*32768 <= gamma <= 1*32768) */
    encop.RcIntervalMode= 1;        /* 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level */
    encop.MbInterval = 0;
    encop.avcIntra16x16OnlyModeEnable = 0;

    encop.ringBufferEnable = enc->ringBufferEnable = 0;
    encop.dynamicAllocEnable = 0;
    encop.chromaInterleave = enc->config->chromaInterleave;

    if(!cpu_is_mx6x() &&  enc->config->format == STD_MJPG )
    {
        qMatTable = (Uint8*)calloc(192,1);
        if (qMatTable == NULL) {
            err_msg("Failed to allocate qMatTable\n");
            return -1;
        }
        huffTable = (Uint8*)calloc(432,1);
        if (huffTable == NULL) {
            free(qMatTable);
            err_msg("Failed to allocate huffTable\n");
            return -1;
        }

        /* Don't consider user defined hufftable this time */
        /* Rearrange and insert pre-defined Huffman table to deticated variable. */
        for(i = 0; i < 16; i += 4)
        {
            huffTable[i] = lumaDcBits[i + 3];
            huffTable[i + 1] = lumaDcBits[i + 2];
            huffTable[i + 2] = lumaDcBits[i + 1];
            huffTable[i + 3] = lumaDcBits[i];
        }
        for(i = 16; i < 32 ; i += 4)
        {
            huffTable[i] = lumaDcValue[i + 3 - 16];
            huffTable[i + 1] = lumaDcValue[i + 2 - 16];
            huffTable[i + 2] = lumaDcValue[i + 1 - 16];
            huffTable[i + 3] = lumaDcValue[i - 16];
        }
        for(i = 32; i < 48; i += 4)
        {
            huffTable[i] = lumaAcBits[i + 3 - 32];
            huffTable[i + 1] = lumaAcBits[i + 2 - 32];
            huffTable[i + 2] = lumaAcBits[i + 1 - 32];
            huffTable[i + 3] = lumaAcBits[i - 32];
        }
        for(i = 48; i < 216; i += 4)
        {
            huffTable[i] = lumaAcValue[i + 3 - 48];
            huffTable[i + 1] = lumaAcValue[i + 2 - 48];
            huffTable[i + 2] = lumaAcValue[i + 1 - 48];
            huffTable[i + 3] = lumaAcValue[i - 48];
        }
        for(i = 216; i < 232; i += 4)
        {
            huffTable[i] = chromaDcBits[i + 3 - 216];
            huffTable[i + 1] = chromaDcBits[i + 2 - 216];
            huffTable[i + 2] = chromaDcBits[i + 1 - 216];
            huffTable[i + 3] = chromaDcBits[i - 216];
        }
        for(i = 232; i < 248; i += 4)
        {
            huffTable[i] = chromaDcValue[i + 3 - 232];
            huffTable[i + 1] = chromaDcValue[i + 2 - 232];
            huffTable[i + 2] = chromaDcValue[i + 1 - 232];
            huffTable[i + 3] = chromaDcValue[i - 232];
        }
        for(i = 248; i < 264; i += 4)
        {
            huffTable[i] = chromaAcBits[i + 3 - 248];
            huffTable[i + 1] = chromaAcBits[i + 2 - 248];
            huffTable[i + 2] = chromaAcBits[i + 1 - 248];
            huffTable[i + 3] = chromaAcBits[i - 248];
        }
        for(i = 264; i < 432; i += 4)
        {
            huffTable[i] = chromaAcValue[i + 3 - 264];
            huffTable[i + 1] = chromaAcValue[i + 2 - 264];
            huffTable[i + 2] = chromaAcValue[i + 1 - 264];
            huffTable[i + 3] = chromaAcValue[i - 264];
        }

        /* Rearrange and insert pre-defined Q-matrix to deticated variable. */
        for(i = 0; i < 64; i += 4)
        {
            qMatTable[i] = lumaQ2[i + 3];
            qMatTable[i + 1] = lumaQ2[i + 2];
            qMatTable[i + 2] = lumaQ2[i + 1];
            qMatTable[i + 3] = lumaQ2[i];
        }
        for(i = 64; i < 128; i += 4)
        {
            qMatTable[i] = chromaBQ2[i + 3 - 64];
            qMatTable[i + 1] = chromaBQ2[i + 2 - 64];
            qMatTable[i + 2] = chromaBQ2[i + 1 - 64];
            qMatTable[i + 3] = chromaBQ2[i - 64];
        }
        for(i = 128; i < 192; i += 4)
        {
            qMatTable[i] = chromaRQ2[i + 3 - 128];
            qMatTable[i + 1] = chromaRQ2[i + 2 - 128];
            qMatTable[i + 2] = chromaRQ2[i + 1 - 128];
            qMatTable[i + 3] = chromaRQ2[i - 128];
        }
    }

    if (enc->config->format == STD_MPEG4) {
        encop.EncStdParam.mp4Param.mp4_dataPartitionEnable = 0;
        enc->mp4_dataPartitionEnable =
            encop.EncStdParam.mp4Param.mp4_dataPartitionEnable;
        encop.EncStdParam.mp4Param.mp4_reversibleVlcEnable = 0;
        encop.EncStdParam.mp4Param.mp4_intraDcVlcThr = 0;
        encop.EncStdParam.mp4Param.mp4_hecEnable = 0;
        encop.EncStdParam.mp4Param.mp4_verid = 2;
    } else if ( enc->config->format == STD_H263) {
        encop.EncStdParam.h263Param.h263_annexIEnable = 0;
        encop.EncStdParam.h263Param.h263_annexJEnable = 1;
        encop.EncStdParam.h263Param.h263_annexKEnable = 0;
        encop.EncStdParam.h263Param.h263_annexTEnable = 0;
    } else if (enc->config->format == STD_AVC) {
        encop.EncStdParam.avcParam.avc_constrainedIntraPredFlag = 0;
        encop.EncStdParam.avcParam.avc_disableDeblk = 0;
        encop.EncStdParam.avcParam.avc_deblkFilterOffsetAlpha = 6;
        encop.EncStdParam.avcParam.avc_deblkFilterOffsetBeta = 0;
        encop.EncStdParam.avcParam.avc_chromaQpOffset = 10;
        encop.EncStdParam.avcParam.avc_audEnable = 0;
        encop.EncStdParam.avcParam.avc_vui_present_flag = 0;
        enc->avc_vui_present_flag = encop.EncStdParam.avcParam.avc_vui_present_flag;
        encop.EncStdParam.avcParam.avc_vui_param.video_signal_type_pres_flag = 1;
        encop.EncStdParam.avcParam.avc_vui_param.video_format = 0;
        encop.EncStdParam.avcParam.avc_vui_param.video_full_range_flag = 1;
        encop.EncStdParam.avcParam.avc_vui_param.colour_descrip_pres_flag = 1;
        encop.EncStdParam.avcParam.avc_vui_param.colour_primaries = 1;
        encop.EncStdParam.avcParam.avc_vui_param.transfer_characteristics = 1;
        encop.EncStdParam.avcParam.avc_vui_param.matrix_coeff = 0;
        encop.EncStdParam.avcParam.avc_level = 0;

        if (cpu_is_mx6x()) {
            encop.EncStdParam.avcParam.interview_en = 0;
            encop.EncStdParam.avcParam.paraset_refresh_en = enc->mvc_paraset_refresh_en = 0;
            encop.EncStdParam.avcParam.prefix_nal_en = 0;
            encop.EncStdParam.avcParam.mvc_extension = enc->config->mp4_h264Class;
            enc->mvc_extension = enc->config->mp4_h264Class;
            encop.EncStdParam.avcParam.avc_frameCroppingFlag = 0;
            encop.EncStdParam.avcParam.avc_frameCropLeft = 0;
            encop.EncStdParam.avcParam.avc_frameCropRight = 0;
            encop.EncStdParam.avcParam.avc_frameCropTop = 0;
            encop.EncStdParam.avcParam.avc_frameCropBottom = 0;
            if (enc->config->rot_angle != 90 &&
                enc->config->rot_angle != 270 &&
                enc->enc_picheight == 1080) {
                /*
                 * In case of AVC encoder, when we want to use
                 * unaligned display width frameCroppingFlag
                 * parameters should be adjusted to displayable
                 * rectangle
                 */
                encop.EncStdParam.avcParam.avc_frameCroppingFlag = 1;
                encop.EncStdParam.avcParam.avc_frameCropBottom = 8;
            }

        } else {
            encop.EncStdParam.avcParam.avc_fmoEnable = 0;
            encop.EncStdParam.avcParam.avc_fmoType = 0;
            encop.EncStdParam.avcParam.avc_fmoSliceNum = 1;
            encop.EncStdParam.avcParam.avc_fmoSliceSaveBufSize = 32; /* FMO_SLICE_SAVE_BUF_SIZE */
        }
    } else if (enc->config->format == STD_MJPG) {
        encop.EncStdParam.mjpgParam.mjpg_sourceFormat = enc->mjpg_fmt; /* encConfig.mjpgChromaFormat */
        encop.EncStdParam.mjpgParam.mjpg_restartInterval = 60;
        encop.EncStdParam.mjpgParam.mjpg_thumbNailEnable = 0;
        encop.EncStdParam.mjpgParam.mjpg_thumbNailWidth = 0;
        encop.EncStdParam.mjpgParam.mjpg_thumbNailHeight = 0;
        if (cpu_is_mx6x()) {
            jpgGetHuffTable(&encop.EncStdParam.mjpgParam);
            jpgGetQMatrix(&encop.EncStdParam.mjpgParam);
            jpgGetCInfoTable(&encop.EncStdParam.mjpgParam);
        } else {
            encop.EncStdParam.mjpgParam.mjpg_hufTable = huffTable;
            encop.EncStdParam.mjpgParam.mjpg_qMatTable = qMatTable;
        }
    }

    ret = vpu_EncOpen(&handle, &encop);
    if (ret != RETCODE_SUCCESS) {
        if (enc->config->format == STD_MJPG) {
            free(qMatTable);
            free(huffTable);
        }
        err_msg("Encoder open failed %d\n", ret);
        return -1;
    }

    enc->handle = handle;
    return 0;
}



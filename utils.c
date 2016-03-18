#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "vrecord.h"
#include <mp4v2/mp4v2.h>

//extern struct video_record *Vrecord;

int quitflag = 0;
char vbuff[512];

/* Our custom header */
struct nethdr {
	int seqno;
	int iframe;
	int len;
};

int	/* write n bytes to a file descriptor */
fwriten(int fd, void *vptr, size_t n)
{
	int nleft;
	int nwrite;
	char  *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwrite = write(fd, ptr, nleft)) <= 0) {
			perror("fwrite: ");
			return (-1);			/* error */
		}

		nleft -= nwrite;
		ptr   += nwrite;
	}

	return (n);
} /* end fwriten */

int	/* Read n bytes from a file descriptor */
freadn(int fd, void *vptr, size_t n)
{
	int nleft = 0;
	int nread = 0;
	char  *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nread = read(fd, ptr, nleft)) <= 0) {
			if (nread == 0)
				return (n - nleft);

			perror("read");
			return (-1);			/* error EINTR XXX */
		}

		nleft -= nread;
		ptr   += nread;
	}

	return (n - nleft);
}

/* Receive data from udp socket */
static int
udp_recv(struct vc_config *config, int sd, char *buf, int n)
{
	int nleft, nread, nactual, nremain, ntotal = 0;
	char *ptr;
	struct nethdr *net_h;
	int hdrlen = sizeof(struct nethdr);
	fd_set rfds;
	struct timeval tv;

	if (config->nlen) {
		/* No more data to be received */
		if (config->nlen == -1) {
			return 0;
		}

		/* There was some data pending from the previous recvfrom.
		 * copy that data into the buffer
		 */
		if (config->nlen > n) {
			memcpy(buf, (config->nbuf + config->noffset), n);
			config->nlen -= n;
			config->noffset += n;
			return n;
		}

		memcpy(buf, (config->nbuf + config->noffset), config->nlen);
		ptr = buf + config->nlen;
		nleft = n - config->nlen;
		ntotal = config->nlen;
		config->nlen = 0;
	} else {
		ptr = buf;
		nleft = n;
	}

	while (nleft > 0) {
		tv.tv_sec = 0;
		tv.tv_usec = 3000;

		FD_ZERO(&rfds);
		FD_SET(sd, &rfds);

		nread = select(sd + 1, &rfds, NULL, NULL, &tv);
		if (nread < 0) {
			perror("select");
			return -1;
		}

		/* timeout */
		if (nread == 0) {
			if (quitflag) {
				n = ntotal;
				break;
			}

			/* wait for complete buffer to be full */
			if (config->complete) {
				continue;
			}

			if (ntotal == 0) {
			       return -EAGAIN;
			}

			n = ntotal;
			break;
		}

		nread = recvfrom(sd, config->nbuf, DEFAULT_PKT_SIZE, 0,
					NULL, NULL);
		if (nread < 0) {
			perror("recvfrom");
			return -1;
		}

		/* get our custom header */
		net_h = (struct nethdr *)config->nbuf;
		dprintf(4, "RX: seqno %d, neth seqno %d, iframe %d, len %d\n",
				config->seq_no, net_h->seqno, net_h->iframe, net_h->len);
		if (net_h->len == 0) {
			/* zero length data means no more data will be
			 * received */
			config->nlen = -1;
			return (n - nleft);
		}

		nactual = (nread - hdrlen);
		if (net_h->len != nactual) {
			warn_msg("length mismatch\n");
		}

		if (config->seq_no++ != net_h->seqno) {
			/* read till we get an I frame */
			if (net_h->iframe == 1) {
				config->seq_no = net_h->seqno + 1;
			} else {
				continue;
			}
		}

		/* check if there is space in user buffer to copy all the
		 * received data
		 */
		if (nactual > nleft) {
			nremain = nleft;
			config->nlen = nactual - nleft;
			config->noffset = (nleft + hdrlen);
		} else {
			nremain = nactual;
		}

		memcpy(ptr, (config->nbuf + hdrlen), nremain);
		ntotal += nremain;
		nleft -= nremain;
		ptr += nremain;
	}

	return (n);
}

/* send data to remote server */
static int
udp_send(struct vc_config *config, int sd, char *buf, int n)
{
	int nwrite, hdrlen;
	struct nethdr net_h;
	struct sockaddr_in addr;

	bzero(&addr, sizeof(addr));
	hdrlen = sizeof(net_h);
	if ((n + hdrlen) > DEFAULT_PKT_SIZE) {
		err_msg("panic: increase default udp pkt size! %d\n", n);
		while (1);
	}

	if (n == 0) {
		net_h.seqno = -1;
		net_h.len = 0;
		memcpy(config->nbuf, (char *)&net_h, hdrlen);
	} else {
		net_h.seqno = config->seq_no++;
		net_h.iframe = config->iframe;
		net_h.len = n;
		memcpy(config->nbuf, (char *)&net_h, hdrlen);
		memcpy((config->nbuf + hdrlen), buf, n);
	}
	dprintf(4, "TX: neth seqno %d, iframe %d, len %d\n", net_h.seqno, net_h.iframe, net_h.len);

	n += hdrlen;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(config->port);
	addr.sin_addr.s_addr = inet_addr(config->output);

	nwrite = sendto(sd, config->nbuf, n, 0, (struct sockaddr *)&addr,
				sizeof(addr));
	if (nwrite != n) {
		err_msg("sendto: error\n");
	}

	return nwrite;
}

int
vpu_read(struct vc_config *config, char *buf, int n)
{
	int fd = config->src_fd;

	if (config->src_scheme == PATH_NET) {
		return udp_recv(config, fd, buf, n);
	}

	return freadn(fd, buf, n);
}

int
vpu_write(struct vc_config *config, char *buf, int n)
{
	int fd = config->dst_fd;
	//int nalsize = n-4;  

	if (config->dst_scheme == PATH_NET) {
		return udp_send(config, fd, buf, n);
	}

#if 0
#if 1
    if (n < 20) {
		for (nalsize = 0; nalsize < n; nalsize++)
		    printf("0x%x ", buf[nalsize]);
		
		printf("\n");
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
        MP4WriteSample(Vrecord->mp4_mux.mp4file, 
            Vrecord->mp4_mux.video_track, 
            (u8 *)buf, n, MP4_INVALID_DURATION, 0, 1);
 }
#endif
    //if (MKVappendFrameData(mobject, buf, n))
		return n;
	//else
		//return -1;
	//return fwriten(fd, buf, n);
}

static char*
skip(char *ptr)
{
	switch (*ptr) {
	case    '\0':
	case    ' ':
	case    '\t':
	case    '\n':
		break;
	case    '\"':
		ptr++;
		while ((*ptr != '\"') && (*ptr != '\0') && (*ptr != '\n')) {
			ptr++;
		}
		if (*ptr != '\0') {
			*ptr++ = '\0';
		}
		break;
	default :
		while ((*ptr != ' ') && (*ptr != '\t')
			&& (*ptr != '\0') && (*ptr != '\n')) {
			ptr++;
		}
		if (*ptr != '\0') {
			*ptr++ = '\0';
		}
		break;
	}

	while ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n')) {
		ptr++;
	}

	return (ptr);
}

void
get_arg(char *buf, int *argc, char *argv[])
{
	char *ptr;
	*argc = 0;

	while ( (*buf == ' ') || (*buf == '\t'))
		buf++;

	for (ptr = buf; *argc < 32; (*argc)++) {
		if (!*ptr)
			break;
		argv[*argc] = ptr + (*ptr == '\"');
		ptr = skip(ptr);
	}

	argv[*argc] = NULL;
}

static int
udp_open(struct vc_config *config)
{
	int sd;
	struct sockaddr_in addr;

	config->nbuf = (char *)malloc(DEFAULT_PKT_SIZE);
	if (config->nbuf == NULL) {
		err_msg("failed to malloc udp buffer\n");
		return -1;
	}

	sd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sd < 0) {
		err_msg("failed to open udp socket\n");
		free(config->nbuf);
		config->nbuf = 0;
		return -1;
	}

	/* If server, then bind */
	if (config->src_scheme == PATH_NET) {
		bzero(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(config->port);
		addr.sin_addr.s_addr = INADDR_ANY;

		if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
			err_msg("udp bind failed\n");
			close(sd);
			free(config->nbuf);
			config->nbuf = 0;
			return -1;
		}

	}

	return sd;
}

int
open_files(struct vc_config *config)
{
	if (config->src_scheme == PATH_FILE) {
#ifdef _FSL_VTS_
    if ( NULL != g_pfnVTSProbe )
    {
        DUT_STREAM_OPEN sFileOpen;
        sFileOpen.strBitstream = g_strInStream;
        sFileOpen.strMode = "rb";
        config->src_fd = NULL;
        config->src_fd = g_pfnVTSProbe( E_OPEN_BITSTREAM, &sFileOpen );
		if (NULL == config->src_fd ) {
			perror("file open");
			return -1;
		}
    }
#else
		config->src_fd = open(config->input, O_RDONLY, 0);
		if (config->src_fd < 0) {
			perror("file open");
			return -1;
		}
		info_msg("Input file \"%s\" opened.\n", config->input);
#endif
	} else if (config->src_scheme == PATH_NET) {
		/* open udp port for receive */
		config->src_fd = udp_open(config);
		if (config->src_fd < 0) {
			return -1;
		}

		info_msg("decoder listening on port %d\n", config->port);
	}

	if (config->dst_scheme == PATH_FILE) {
		config->dst_fd = open(config->output, O_CREAT | O_RDWR | O_TRUNC,
					S_IRWXU | S_IRWXG | S_IRWXO);
		if (config->dst_fd < 0) {
			perror("file open");

			if (config->src_scheme == PATH_FILE)
				close(config->src_fd);

			return -1;
		}
		info_msg("Output file \"%s\" opened.\n", config->output);
	} else if (config->dst_scheme == PATH_NET) {
		/* open udp port for send path */
		config->dst_fd = udp_open(config);
		if (config->dst_fd < 0) {
			if (config->src_scheme == PATH_NET)
				close(config->src_fd);
			return -1;
		}

		info_msg("encoder sending on port %d\n", config->port);
	}

	return 0;
}

void
close_files(struct vc_config *config)
{
	
	if ((config->src_fd > 0)) {
		close(config->src_fd);
		config->src_fd = -1;
	}

	if ((config->dst_fd > 0)) {
		close(config->dst_fd);
		config->dst_fd = -1;
	}

	if (config->nbuf) {
		free(config->nbuf);
		config->nbuf = 0;
	}
}

int
check_params(struct vc_config *config, int op)
{
	switch (config->format) {
	case STD_MPEG4:
		info_msg("Format: STD_MPEG4\n");
		switch (config->mp4_h264Class) {
		case 0:
			info_msg("MPEG4 class: MPEG4\n");
			break;
		case 1:
			info_msg("MPEG4 class: DIVX5.0 or higher\n");
			break;
		case 2:
			info_msg("MPEG4 class: XVID\n");
			break;
		case 5:
			info_msg("MPEG4 class: DIVX4.0\n");
			break;
		default:
			err_msg("Unsupported MPEG4 Class!\n");
			break;
		}
		break;
	case STD_H263:
		info_msg("Format: STD_H263\n");
		break;
	case STD_AVC:
		info_msg("Format: STD_AVC\n");
		switch (config->mp4_h264Class) {
		case 0:
			info_msg("AVC\n");
			break;
		case 1:
			info_msg("MVC\n");
			break;
		default:
			err_msg("Unsupported H264 type\n");
			break;
		}
		break;
	case STD_VC1:
		info_msg("Format: STD_VC1\n");
		break;
	case STD_MPEG2:
		info_msg("Format: STD_MPEG2\n");
		break;
	case STD_DIV3:
		info_msg("Format: STD_DIV3\n");
		break;
	case STD_RV:
		info_msg("Format: STD_RV\n");
		break;
	case STD_MJPG:
		info_msg("Format: STD_MJPG\n");
		break;
	case STD_AVS:
		info_msg("Format: STD_AVS\n");
		break;
	case STD_VP8:
		info_msg("Format: STD_VP8\n");
		break;
	default:
		err_msg("Unsupported Format!\n");
		break;
	}

	if (config->port == 0) {
		config->port = DEFAULT_PORT;
	}

	if (config->src_scheme != PATH_FILE && op == DECODE) {
		config->src_scheme = PATH_NET;
	}

	if (config->src_scheme == PATH_FILE && op == ENCODE) {
		if (config->width == 0 || config->height == 0) {
			warn_msg("Enter width and height for YUV file\n");
			return -1;
		}
	}

	if (config->src_scheme == PATH_V4L2 && op == ENCODE) {
		if (config->width == 0)
			config->width = 176; /* default */

		if (config->height == 0)
			config->height = 144;

		if (config->width % 16 != 0) {
			config->width -= config->width % 16;
			warn_msg("width not divisible by 16, adjusted %d\n",
					config->width);
		}

		if (config->height % 8 != 0) {
			config->height -= config->height % 8;
			warn_msg("height not divisible by 8, adjusted %d\n",
					config->height);
		}

	}

	if (config->dst_scheme != PATH_FILE && op == ENCODE) {
		if (config->dst_scheme != PATH_NET) {
			warn_msg("No output file specified, using default\n");
			config->dst_scheme = PATH_FILE;

			if (config->format == STD_MPEG4) {
				strncpy(config->output, "enc.mpeg4", 16);
			} else if (config->format == STD_H263) {
				strncpy(config->output, "enc.263", 16);
			} else {
				strncpy(config->output, "enc.264", 16);
			}
		}
	}

	if (config->rot_en) {
		if (config->rot_angle != 0 && config->rot_angle != 90 &&
			config->rot_angle != 180 && config->rot_angle != 270) {
			warn_msg("Invalid rotation angle. No rotation!\n");
			config->rot_en = 0;
			config->rot_angle = 0;
		}
	}

	if (config->mirror < MIRDIR_NONE || config->mirror > MIRDIR_HOR_VER) {
		warn_msg("Invalid mirror angle. Using 0\n");
		config->mirror = 0;
	}

	if (!(config->format == STD_MPEG4 || config->format == STD_H263 ||
	    config->format == STD_MPEG2 || config->format == STD_DIV3) &&
	    config->deblock_en) {
		warn_msg("Deblocking only for MPEG4 and MPEG2. Disabled!\n");
		config->deblock_en = 0;
	}

	return 0;
}

char*
skip_unwanted(char *ptr)
{
	int i = 0;
	static char buf[MAX_PATH];
	while (*ptr != '\0') {
		if (*ptr == ' ' || *ptr == '\t' || *ptr == '\n') {
			ptr++;
			continue;
		}

		if (*ptr == '#')
			break;

		buf[i++] = *ptr;
		ptr++;
	}

	buf[i] = 0;
	return (buf);
}

int parse_options(char *buf, struct vc_config *config, int *mode)
{
	char *str;

	str = strstr(buf, "end");
	if (str != NULL) {
		return 100;
	}

	str = strstr(buf, "operation");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				*mode = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "input");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				strncpy(config->input, str, MAX_PATH);
				config->src_scheme = PATH_FILE;
			}
		}

		return 0;
	}

	str = strstr(buf, "output");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				strncpy(config->output, str, MAX_PATH);
				config->dst_scheme = PATH_FILE;
			}
		}

		return 0;
	}

	str = strstr(buf, "port");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->port = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "format");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->format = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "rotation");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->rot_en = 1;
				config->rot_angle = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "ext_rot");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->ext_rot_en = strtol(str, NULL, 10);
				if (config->ext_rot_en == 1)
					config->rot_en = 0;
			}
		}

		return 0;
	}

        str = strstr(buf, "ip");
        if (str != NULL) {
                str = index(buf, '=');
                if (str != NULL) {
                        str++;
                        if (*str != '\0') {
                                strncpy(config->output, str, 64);
                                config->dst_scheme = PATH_NET;
                        }
                }

                return 0;
        }

	str = strstr(buf, "count");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->count = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "chromaInterleave");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->chromaInterleave = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "mp4Class");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->mp4_h264Class = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "deblock");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->deblock_en = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "dering");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->dering_en = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "mirror");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->mirror = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "width");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->width = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "height");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->height = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "bitrate");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->bitrate = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "prescan");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
					config->bs_mode = strtol(str, NULL, 10);
					config->prescan = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "gop");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->gop = strtol(str, NULL, 10);
			}
		}

		return 0;
	}

	str = strstr(buf, "quantParam");
	if (str != NULL) {
		str = index(buf, '=');
		if (str != NULL) {
			str++;
			if (*str != '\0') {
				config->quantParam = strtol(str, NULL, 10);
			}
		}

		return 0;
        }

	return 0;
}

void SaveQpReport(Uint32 *qpReportAddr, int picWidth, int picHeight,
		  int frameIdx, char *fileName)
{
	FILE *fp;
	int i, j;
	int MBx, MBy, MBxof4, MBxof1, MBxx;
	Uint32 *qpAddr;
	Uint32 qp;
	Uint8 lastQp[4];

	if (frameIdx == 0)
		fp = fopen(fileName, "wb");
	else
		fp = fopen(fileName, "a+b");

	if (!fp) {
		err_msg("Can't open %s in SaveQpReport\n", fileName);
		return;
	}

	MBx = picWidth / 16;
	MBxof1 = MBx % 4;
	MBxof4 = MBx - MBxof1;
	MBy = picHeight / 16;
	MBxx = (MBx + 3) / 4 * 4;
	for (i = 0; i < MBy; i++) {
		for (j = 0; j < MBxof4; j = j + 4) {
			dprintf(3, "qpReportAddr = %lx\n", (Uint32)qpReportAddr);
			qpAddr = (Uint32 *)((Uint32)qpReportAddr + j + MBxx * i);
			qp = *qpAddr;
			fprintf(fp, " %4d %4d %3d \n", frameIdx,
				MBx * i + j + 0, (Uint8) (qp >> 24));
			fprintf(fp, " %4d %4d %3d \n", frameIdx,
				MBx * i + j + 1, (Uint8) (qp >> 16));
			fprintf(fp, " %4d %4d %3d \n", frameIdx,
				MBx * i + j + 2, (Uint8) (qp >> 8));
			fprintf(fp, " %4d %4d %3d \n", frameIdx,
				MBx * i + j + 3, (Uint8) qp);
		}

		if (MBxof1 > 0) {
			qpAddr = (Uint32 *)((Uint32)qpReportAddr + MBxx * i + MBxof4);
			qp = *qpAddr;
			lastQp[0] = (Uint8) (qp >> 24);
			lastQp[1] = (Uint8) (qp >> 16);
			lastQp[2] = (Uint8) (qp >> 8);
			lastQp[3] = (Uint8) (qp);
		}

		for (j = 0; j < MBxof1; j++) {
			fprintf(fp, " %4d %4d %3d \n", frameIdx,
				MBx * i + j + MBxof4, lastQp[j]);
		}
	}

	fclose(fp);
}

char *get_the_filename(char *parent_dir)
{
	char tbuff[200];
	time_t now;
    struct tm *tm_now;

    time(&now);
    tm_now = localtime(&now);
    strftime(tbuff, 200,"%Y-%m-%d_%H-%M-%S", tm_now); 
    //sprintf(vbuff, "%s/video_%s.h264", parent_dir, tbuff);
    sprintf(vbuff, "%s/video_%s.mp4", parent_dir, tbuff);
    //printf("creat the file %s now.\n", vbuff);
    return vbuff;
}

int create_vfile(char * path)
{
	int fp;
	char tbuff[200];
	char buff[512];
	time_t now;
    struct tm *tm_now;
    
    time(&now);
    tm_now = localtime(&now);
    strftime(tbuff, 200,"%Y-%m-%d_%H:%M:%S", tm_now); 
    sprintf(buff, "%s/video_%s.mvk",path, tbuff);
    printf("creat the file %s now.\n", buff);
     
    fp = open(buff, O_CREAT | O_RDWR , 0664);

    if (fp < 0) 
        perror("create file fail");
    else
        close(fp);
          
    return fp;
}

int check_and_make_workdir(char * workdir)
{
    int ret;
    //struct stat buf;

    ret = access(workdir, F_OK);
    if (ret) {
        info_msg("The dir %s doesn't exist, create it now.\n", workdir);
        ret = mkdir(workdir, DIR_FLAG);
        if (ret)  {
            perror("Create work-dir fail");
            ret = VR_CHECKDIR_ERR;
        }
    }

    return ret;
}
  
int check_and_make_subdir(char * workdir, char *subdir_prefix, int channel)
{
    int ret;
    char path_buff[256];
    
    ret = chdir(workdir);
	if (ret)
        return VR_CHDIR_ERR;

    sprintf(path_buff, "%s%d", subdir_prefix, channel);
    ret = access(path_buff, F_OK);
    if (ret) {
        info_msg("The dir %s doesn't exist, create it now.\n", path_buff);
        ret = mkdir(path_buff, DIR_FLAG);
        if (ret) {
            perror("Create sub-dir fail");
		    return VR_CHECKDIR_ERR;
		}
    }

    return VR_OK;
}



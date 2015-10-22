/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#if defined(__WIN32__) || defined(WIN32)

#define  __USE_W32_SOCKETS
//#define Win32_Winsock

#include <windows.h>
#include <stdio.h>
#include <initguid.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define in_addr_t uint32_t
#else /* UNIX */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#endif /* WIN32 */

#include <time.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <glib.h>

#if (GTKVER == 3)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#ifdef WIN32
#include <process.h>
//#include <glib/gwin32.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

/* System-dependent definitions */
#ifndef WIN32
#define closesocket     close
#define SOCKET          gint
#define INVALID_SOCKET  -1
#define SOCKET_ERROR    -1
#define SOCKADDR_BTH    struct sockaddr_rc
#define AF_BTH          PF_BLUETOOTH
#define BTHPROTO_RFCOMM BTPROTO_RFCOMM
#endif

#include "judoproxy.h"

GtkWidget *camera_image = NULL;

/* FFMpeg vbls */
static AVFormatContext *pFormatCtx = NULL;
static AVCodecContext *pCodecCtx = NULL, *pCodecCtxOrig = NULL;
static int videoStream;
static struct SwsContext *sws_ctx = NULL;
static AVCodec *pCodec = NULL;
static AVFrame *picture_RGB;
static AVFrame *pFrame = NULL;
static int width = 0, height = 0, stride, framenum = 0;
static unsigned char picture[640*400*4];
static uint32_t current_addr;
uint32_t camera_addr;
gint pic_button_x1, pic_button_y1, pic_button_x2, pic_button_y2;
gboolean pic_button_drag = FALSE;

void pic_button_released(gint x, gint y)
{

}

void set_camera_addr(uint32_t addr)
{
    camera_addr = addr;
}

gboolean show_camera_video(gpointer arg)
{
    static int lastframe = 0;
    GdkPixbuf *pixbuf;

    if (!show_video || !width || !camera_image || framenum == lastframe)
	return TRUE;

    lastframe = framenum;
    pixbuf = gdk_pixbuf_new_from_data(picture, GDK_COLORSPACE_RGB,
				      0, 8, width, height,
				      stride,
				      NULL, NULL);
    if (!pixbuf)
	return TRUE;

    if (pic_button_drag) {
	int x, y;
	guchar *p = gdk_pixbuf_get_pixels(pixbuf);
	gint p_stride = gdk_pixbuf_get_rowstride(pixbuf);

	for (x = pic_button_x1; x < pic_button_x2; x++)
	    p[x*3 + pic_button_y1*p_stride] = 0xff;
	for (x = pic_button_x1; x < pic_button_x2; x++)
	    p[x*3 + pic_button_y2*p_stride] = 0xff;
	for (y = pic_button_y1; y < pic_button_y2; y++)
	    p[pic_button_x1*3 + y*p_stride] = 0xff;
	for (y = pic_button_y1; y < pic_button_y2; y++)
	    p[pic_button_x2*3 + y*p_stride] = 0xff;
    }

    gtk_image_set_from_pixbuf((GtkImage*) camera_image, pixbuf);
    g_object_unref(pixbuf);
    return TRUE;
}

static int read_function(void* opaque, uint8_t* buf, int buf_size)
{
    int *fdp = opaque;
    int fd = *fdp;
    fd_set read_fd;
    struct timeval timeout;
    gint r;

    if (fd == 0)
	return 0;

    FD_ZERO(&read_fd);
    FD_SET(fd, &read_fd);

    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    r = select(fd+1, &read_fd, NULL, NULL, &timeout);
    if (r <= 0)
	return 0;

    if (FD_ISSET(fd, &read_fd)) {
	int n = recv(fd, buf, buf_size, 0);
	if (n < 0) {
            closesocket(fd);
	    *fdp = 0;
	    return 0;
	}
	return n;
    }

    return 0;
}

gpointer camera_video(gpointer args)
{
    AVDictionary *optionsDict = NULL;
    char *buffer;
    int comm_fd;
    struct sockaddr_in node;

    av_register_all();

    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
	int i;

	while (!camera_addr || !show_video) {
                g_usleep(1000000);
	}

	current_addr = camera_addr;

        if ((comm_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            perror("serv socket");
            g_print("CANNOT CREATE SOCKET (%s:%d)!\n", __FUNCTION__, __LINE__);
            g_thread_exit(NULL);    /* not required just good pratice */
            return NULL;
        }

        memset(&node, 0, sizeof(node));
        node.sin_family      = AF_INET;
        node.sin_port        = htons(8889);
        node.sin_addr.s_addr = camera_addr;

        if (connect(comm_fd, (struct sockaddr *)&node, sizeof(node))) {
            closesocket(comm_fd);
	    comm_fd = 0;
            g_usleep(1000000);
            continue;
        }

        g_print("Connection OK.\n");
	fflush(NULL);
	const int ioBufferSize = 32768;
	unsigned char * ioBuffer = (unsigned char *)av_malloc
		(ioBufferSize + FF_INPUT_BUFFER_PADDING_SIZE); // can get av_free()ed by libav
	AVIOContext * avioContext = avio_alloc_context
		(ioBuffer, ioBufferSize, 0, &comm_fd,
		 &read_function, NULL, NULL /*&seekFunction*/);
	pFormatCtx = avformat_alloc_context();
	pFormatCtx->pb = avioContext;
	avformat_open_input(&pFormatCtx, "dummyFileName.h264", NULL, NULL);

	if (!pFormatCtx)
	    goto err;

	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	    goto err; // Couldn't find stream information

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, "Camera", 0);

	// Find the first video stream
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
		videoStream = i;
		break;
	    }
	if (videoStream == -1)
	    goto err; // Didn't find a video stream

	for (i = 0; i < pFormatCtx->nb_streams; i++)
	    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) {
		g_print("Found an audio stream too\n");
		break;
	    }

	// Get a pointer to the codec context for the video stream
	pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if (pCodec == NULL) {
	    g_print("Unsupported codec!\n");
	    goto err; // Codec not found
	}
	// Copy context
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		g_print("Couldn't copy codec context");
		goto err; // Error copying codec context
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0)
	    goto err; // Could not open codec

	sws_ctx =
	    sws_getContext
	    (pCodecCtx->width,
	     pCodecCtx->height,
	     pCodecCtx->pix_fmt,
	     pCodecCtx->width,
	     pCodecCtx->height,
	     PIX_FMT_RGB24 /*PIX_FMT_YUV420P*/,
	     SWS_BILINEAR,
	     NULL,
	     NULL,
	     NULL);

	/* based on code from
	   http://www.cs.dartmouth.edu/~xy/cs23/gtk.html
	   http://cdry.wordpress.com/2009/09/09/using-custom-io-callbacks-with-ffmpeg/
	*/

	AVPacket packet;
	int frameFinished;

	/* initialize packet, set data to NULL, let the demuxer fill it */
	/* http://ffmpeg.org/doxygen/trunk/doc_2examples_2demuxing_8c-example.html#a80 */
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;

	int bufsize = avpicture_get_size(PIX_FMT_RGB24,
					 pCodecCtx->width, pCodecCtx->height);
	buffer = av_malloc(bufsize);

	pFrame = avcodec_alloc_frame();

	picture_RGB = avcodec_alloc_frame();
	avpicture_fill((AVPicture *)picture_RGB, (uint8_t *)buffer, PIX_FMT_RGB24,
		       pCodecCtx->width, pCodecCtx->height);

	while (av_read_frame(pFormatCtx, &packet) >= 0 &&
	       camera_addr == current_addr) {
	    //g_print("packet.stream_index=%d\n", packet.stream_index);
	    if (packet.stream_index == videoStream) {
		//usleep(40000);
		// Decode video frame
		avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
				      &packet);

		width = pCodecCtx->width;
		height = pCodecCtx->height;

		sws_ctx = sws_getContext(width, height, pCodecCtx->pix_fmt, width, height,
					 PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

		if (frameFinished) {
#if 0
		    g_print("Frame %d width=%d, height=%d, pFrame->linesize[0]=%d\n", i++,
			    width, height, pFrame->linesize[0]);
#endif
		    sws_scale(sws_ctx,
			      (uint8_t const * const *)pFrame->data,
			      pFrame->linesize, 0,
			      height, picture_RGB->data, picture_RGB->linesize);

		    //picture = picture_RGB->data[0];
		    stride = picture_RGB->linesize[0];
		    memcpy(picture, picture_RGB->data[0], stride*height);

		    // Drawing is done as a background job.
		    framenum++;
		}
	    }
	    av_free_packet(&packet);
	} // while

    err:
	//picture = NULL;
	width = 0;
	closesocket(comm_fd);
	comm_fd = 0;

	// Free the RGB image
	av_free(buffer);
	//avcodec_free_frame(&picture_RGB);

	// Free the YUV frame
	//avcodec_free_frame(&pFrame);

	// Close the codecs
	avcodec_close(pCodecCtx);
	avcodec_close(pCodecCtxOrig);

	// Close the video file
	//avformat_close_input(&pFormatCtx);

	//sws_freeContext(sws_ctx);

	g_print("Connection closed.\n");
	g_usleep(1000000);
    } // for ever

    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}

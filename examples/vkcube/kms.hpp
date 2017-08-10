#pragma once

#include <sys/sysmacros.h>

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/major.h>
#include <termios.h>
#include <poll.h>
#include <math.h>
#include <assert.h>
#include <sys/mman.h>
#include <linux/input.h>

static void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *data)
{}

static struct termios save_tio;

class VikDisplayModeKMS {

    drmModeCrtc *crtc;
    drmModeConnector *connector;

public:
    VikDisplayModeKMS() {}
    ~VikDisplayModeKMS() {}

    static void
    restore_vt(void)
    {
	struct vt_mode mode = { .mode = VT_AUTO };
	ioctl(STDIN_FILENO, VT_SETMODE, &mode);

	tcsetattr(STDIN_FILENO, TCSANOW, &save_tio);
	ioctl(STDIN_FILENO, KDSETMODE, KD_TEXT);
    }

    static void
    handle_signal(int sig)
    {
	restore_vt();
    }

    void main_loop(struct vkcube *vc)
    {
	int len, ret;
	char buf[16];
	struct pollfd pfd[2];
	struct vkcube_buffer *b;

	pfd[0].fd = STDIN_FILENO;
	pfd[0].events = POLLIN;
	pfd[1].fd = vc->kms.fd;
	pfd[1].events = POLLIN;

	drmEventContext evctx = {};
	evctx.version = 2;
	evctx.page_flip_handler = page_flip_handler;

	ret = drmModeSetCrtc(vc->kms.fd, crtc->crtc_id, vc->buffers[0].fb,
	        0, 0, &connector->connector_id, 1, &crtc->mode);
	fail_if(ret < 0, "modeset failed: %m\n");


	ret = drmModePageFlip(vc->kms.fd, crtc->crtc_id, vc->buffers[0].fb,
	        DRM_MODE_PAGE_FLIP_EVENT, NULL);
	fail_if(ret < 0, "pageflip failed: %m\n");

	while (1) {
	    ret = poll(pfd, 2, -1);
	    fail_if(ret == -1, "poll failed\n");
	    if (pfd[0].revents & POLLIN) {
		len = read(STDIN_FILENO, buf, sizeof(buf));
		switch (buf[0]) {
		    case 'q':
			return;
		    case '\e':
			if (len == 1)
			    return;
		}
	    }
	    if (pfd[1].revents & POLLIN) {
		drmHandleEvent(vc->kms.fd, &evctx);
		b = &vc->buffers[vc->current & 1];
		vc->model.render(vc, b);

		ret = drmModePageFlip(vc->kms.fd, crtc->crtc_id, b->fb,
		                      DRM_MODE_PAGE_FLIP_EVENT, NULL);
		fail_if(ret < 0, "pageflip failed: %m\n");
		vc->current++;
	    }
	}
    }

    int
    init_vt(struct vkcube *vc)
    {

	printf("kms init vt\n");

	struct termios tio;
	struct stat buf;
	int ret;

	/* First, save term io setting so we can restore properly. */
	tcgetattr(STDIN_FILENO, &save_tio);

	/* Make sure we're on a vt. */
	ret = fstat(STDIN_FILENO, &buf);
	fail_if(ret == -1, "failed to stat stdin\n");

	if (major(buf.st_rdev) != TTY_MAJOR) {
	    fprintf(stderr, "stdin not a vt, running in no-display mode\n");
	    return -1;
	}

	atexit(restore_vt);

	/* Set console input to raw mode. */
	tio = save_tio;
	tio.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &tio);

	/* Restore console on SIGINT and friends. */
	struct sigaction act = {};
	act.sa_handler = handle_signal;
	act.sa_flags = (int) SA_RESETHAND;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGABRT, &act, NULL);

	/* We don't drop drm master, so block VT switching while we're
    * running. Otherwise, switching to X on another VT will crash X when it
    * fails to get drm master. */
	struct vt_mode mode = {};
	mode.mode = VT_PROCESS;
	mode.relsig = 0;
	mode.acqsig = 0;
	ret = ioctl(STDIN_FILENO, VT_SETMODE, &mode);
	fail_if(ret == -1, "failed to take control of vt handling\n");

	/* Set KD_GRAPHICS to disable fbcon while we render. */
	ret = ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS);
	fail_if(ret == -1, "failed to switch console to graphics mode\n");

	return 0;
    }

    // Return -1 on failure.
    int
    init(struct vkcube *vc)
    {

	printf("kms init\n");

	drmModeRes *resources;
	drmModeEncoder *encoder;
	int i;

	if (init_vt(vc) == -1)
	    return -1;

	vc->kms.fd = open("/dev/dri/card0", O_RDWR);
	fail_if(vc->kms.fd == -1, "failed to open /dev/dri/card0\n");

	/* Get KMS resources and find the first active connecter. We'll use that
      connector and the crtc driving it in the mode it's currently running. */
	resources = drmModeGetResources(vc->kms.fd);
	fail_if(!resources, "drmModeGetResources failed: %s\n", strerror(errno));

	for (i = 0; i < resources->count_connectors; i++) {
	    connector = drmModeGetConnector(vc->kms.fd, resources->connectors[i]);
	    if (connector->connection == DRM_MODE_CONNECTED)
		break;
	    drmModeFreeConnector(connector);
	    connector = NULL;
	}

	fail_if(!connector, "no connected connector!\n");
	encoder = drmModeGetEncoder(vc->kms.fd, connector->encoder_id);
	fail_if(!encoder, "failed to get encoder\n");
	crtc = drmModeGetCrtc(vc->kms.fd, encoder->crtc_id);
	fail_if(!crtc, "failed to get crtc\n");
	printf("mode info: hdisplay %d, vdisplay %d\n",
	       crtc->mode.hdisplay, crtc->mode.vdisplay);

	vc->width = crtc->mode.hdisplay;
	vc->height = crtc->mode.vdisplay;

	vc->kms.gbm_device = gbm_create_device(vc->kms.fd);

	init_vk(vc, NULL);
	vc->image_format = VK_FORMAT_R8G8B8A8_SRGB;
	init_vk_objects(vc);

	PFN_vkCreateDmaBufImageINTEL create_dma_buf_image =
	        (PFN_vkCreateDmaBufImageINTEL)vkGetDeviceProcAddr(vc->device, "vkCreateDmaBufImageINTEL");

	for (uint32_t i = 0; i < 2; i++) {
	    struct vkcube_buffer *b = &vc->buffers[i];
	    int fd, stride, ret;

	    b->gbm_bo = gbm_bo_create(vc->kms.gbm_device, vc->width, vc->height,
	                              GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT);

	    fd = gbm_bo_get_fd(b->gbm_bo);
	    stride = gbm_bo_get_stride(b->gbm_bo);


	    VkDmaBufImageCreateInfo dmaBufInfo = {};

	    VkExtent3D extent = {};
	    extent.width = vc->width;
	    extent.height = vc->height;
	    extent.depth = 1;

	    dmaBufInfo.sType = (VkStructureType) VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL;
	    dmaBufInfo.fd = fd;
	    dmaBufInfo.format = vc->image_format;
	    dmaBufInfo.extent = extent;
	    dmaBufInfo.strideInBytes = stride;

	    create_dma_buf_image(vc->device,
	                         &dmaBufInfo,
	                         NULL,
	                         &b->mem,
	                         &b->image);
	    close(fd);

	    b->stride = gbm_bo_get_stride(b->gbm_bo);
	    uint32_t bo_handles[4] = { (uint32_t) (gbm_bo_get_handle(b->gbm_bo).s32), };
	    uint32_t pitches[4] = { (uint32_t) stride, };
	    uint32_t offsets[4] = { 0, };
	    ret = drmModeAddFB2(vc->kms.fd, vc->width, vc->height,
	                        DRM_FORMAT_XRGB8888, bo_handles,
	                        pitches, offsets, &b->fb, 0);
	    fail_if(ret == -1, "addfb2 failed\n");

	    init_buffer(vc, b);
	}

	return 0;
    }
};

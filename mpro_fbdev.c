/* SPDX-License-Identifier: MIT */
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_print.h>
#include <drm/drm_rect.h>
#include <linux/usb.h>
#include "mpro.h"

static char cmd_draw[12] = {
	0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int mpro_blit(struct mpro_device *mpro, struct drm_rect* rect) {

	int cmd_len = 6;

	// partial frame update
	if ( rect -> x1 != 0 || rect -> y1 != 0 || rect -> x2 != mpro -> info.rect.x2 || rect -> y2 != mpro -> info.rect.y2 ) {

		int len = (rect -> x2 - rect -> x1) * (rect -> y2 - rect -> y1) * MPRO_BPP / 8;
		int width = rect -> x2 - rect -> x1;

		cmd_draw[2] = (char)(len >> 0);
		cmd_draw[3] = (char)(len >> 8);
		cmd_draw[4] = (char)(len >> 16);

		cmd_draw[6] = (char)(rect -> x1 >> 0);
		cmd_draw[7] = (char)(rect -> x1 >> 8);
		cmd_draw[8] = (char)(rect -> y1 >> 0);
		cmd_draw[9] = (char)(rect -> y1 >> 8);
		cmd_draw[10] = (char)(width >> 0);
		cmd_draw[11] = (char)(width >> 8);

		cmd_len = 12;

	} else { // fullscreen frame update

		cmd_draw[2] = (char)(mpro -> block_size >> 0);
		cmd_draw[3] = (char)(mpro -> block_size >> 8);
		cmd_draw[4] = (char)(mpro -> block_size >> 16);
	}

	struct usb_device *udev = mpro_to_usb_device(mpro);
	int ret;

	/* 0x40 USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE */
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xb0, 0x40,
			      0, 0, cmd_draw, cmd_len,
			      MPRO_MAX_DELAY);
	if ( ret < 0 )
		return ret;

	ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, 0x02), mpro -> data,
			   mpro ->block_size, NULL, MPRO_MAX_DELAY);

	return ret < 0 ? ret : 0;
}

int mpro_fbdev_setup(struct mpro_device *mpro, unsigned int preferred_bpp) {

	cmd_draw[2] = (char)(mpro -> block_size >> 0);
	cmd_draw[3] = (char)(mpro -> block_size >> 8);
	cmd_draw[4] = (char)(mpro -> block_size >> 16);

	int ret = drm_dev_register(&mpro -> dev, 0);
	if ( ret )
		return ret;

	drm_fbdev_generic_setup(&mpro -> dev, preferred_bpp);
	return 0;
}

/* SPDX-License-Identifier: MIT */
#include <linux/usb.h>
#include "mpro.h"

static char cmd_draw[12] = {
	0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int mpro_blit(struct mpro_device *mpro) {

	struct usb_device *udev = mpro_to_usb_device(mpro);
	//int cmd_len = mpro -> partial < 1 ? 6 : 12;
	int cmd_len = 6;
	int ret;

	/* 0x40 USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE */
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xb0, 0x40,
			      0, 0, cmd_draw, cmd_len,
			      MPRO_MAX_DELAY);
	if ( ret < 0 )
		return ret;

	ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, 0x02), mpro -> data,
			   mpro ->block_size, NULL, MPRO_MAX_DELAY);
	if ( ret < 0 )
		return ret;

	return 0;
}

void mpro_prepare(struct mpro_device *mpro) {

	cmd_draw[2] = (char)(mpro->block_size >> 0);
	cmd_draw[3] = (char)(mpro->block_size >> 8);
	cmd_draw[4] = (char)(mpro->block_size >> 16);
}

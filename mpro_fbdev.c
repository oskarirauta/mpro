/* SPDX-License-Identifier: MIT */
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_print.h>
#include <linux/usb.h>
#include "mpro.h"

static char cmd_draw[12] = {
	0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int mpro_blit(struct mpro_device *mpro) {

	struct usb_device *udev = mpro_to_usb_device(mpro);
	//int cmd_len = mpro -> config.partial < 1 ? 6 : 12;
	//int cmd_len = 6;
	int ret;

	/* 0x40 USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE */
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0xb0, 0x40,
			      0, 0, cmd_draw, mpro -> config.partial < 1 ? 6 : 12,
			      MPRO_MAX_DELAY);
	if ( ret < 0 )
		return ret;

	ret = usb_bulk_msg(udev, usb_sndbulkpipe(udev, 0x02), mpro -> data,
			   mpro ->block_size, NULL, MPRO_MAX_DELAY);

	return ret < 0 ? ret : 0;
}

static int mpro_fbdev_helper_fb_probe(struct drm_fb_helper *fb_helper, struct drm_fb_helper_surface_size *sizes) {
	return 0;
}

static int mpro_fbdev_helper_fb_dirty(struct drm_fb_helper *helper, struct drm_clip_rect *clip) {

	struct drm_device *dev = helper -> dev;
	struct mpro_device *mpro = to_mpro(dev);

	/* Call damage handlers only if necessary */
	if ( !( clip -> x1 < clip -> x2 && clip -> y1 < clip -> y2 ))
		return 0;
/*
	if ( mpro -> config.flipx )
		drm_fb_xrgb8888_to_rgb565_flipped(&mpro -> screen_base, &mpro -> pitch, shadow_plane_state -> data, fb, &damage, false);
	else
		drm_fb_xrgb8888_to_rgb565(&mpro -> screen_base, &mpro -> pitch, shadow_plane_state -> data, fb, &damage, false);
*/
	// fullscreen only
	mpro_blit(mpro);

	return 0;
}

static const struct drm_fb_helper_funcs mpro_fb_helper_funcs = {
	.fb_probe = mpro_fbdev_helper_fb_probe,
	.fb_dirty = mpro_fbdev_helper_fb_dirty,
};


int mpro_fbdev_setup(struct mpro_device *mpro, unsigned int preferred_bpp) {

	cmd_draw[2] = (char)(mpro -> block_size >> 0);
	cmd_draw[3] = (char)(mpro -> block_size >> 8);
	cmd_draw[4] = (char)(mpro -> block_size >> 16);

	int ret = drm_dev_register(&mpro -> dev, 0);
	if ( ret )
		return ret;

	drm_fbdev_generic_setup(&mpro -> dev, preferred_bpp);

	return 0;

	if ( !mpro -> dev.registered ) {
		drm_err(&mpro -> dev, "device has not registered");
		return -ENODEV;
	}

	mpro -> dev.fb_helper -> funcs = &mpro_fb_helper_funcs;
	return 0;
}

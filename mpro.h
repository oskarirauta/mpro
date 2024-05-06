/* SPDX-License-Identifier: MIT */
#ifndef _MPRO_H_
#define _MPRO_H_

#include <linux/platform_device.h>
#include <linux/platform_data/simplefb.h>
#include <linux/iosys-map.h>
#include <linux/usb.h>

#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_modes.h>
#include <drm/drm_plane.h>
#include <drm/drm_format_helper.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <drm/drm_crtc.h>
#include <drm/drm_rect.h>

// We only support rgb565 though..
#define MPRO_FORMATS \
{ \
	{ "r5g6b5", 16, {11, 5}, {5, 6}, {0, 5}, {0, 0}, DRM_FORMAT_RGB565 }, \
	{ "x8r8g8b8", 32, {16, 8}, {8, 8}, {0, 8}, {0, 0}, DRM_FORMAT_XRGB8888 }, \
}

#define MPRO_BPP	16
#define MPRO_MAX_DELAY	100

struct mpro_format {
	const char *name;
	u32 bits_per_pixel;
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
	u32 fourcc;
};

struct mpro_info {
	char* model;
	int width;
	int height;
	int width_mm;
	int height_mm;
	int margin;
	int hz;
	int stride;
	struct drm_rect rect;
};

struct mpro_config {
	char flipx;
	char partial;
};

struct mpro_device {
	struct drm_device dev;
	struct device *dmadev;

	/* simplefb settings */
	struct drm_display_mode mode;
	const struct drm_format_info *format;
	unsigned int pitch; // pixels on line!!

	/* memory management */
	struct iosys_map screen_base;
	unsigned char* data;
	unsigned int block_size;

	/* modesetting */
	uint32_t formats[8];
	struct drm_plane primary_plane;
	struct drm_crtc crtc;
	struct drm_encoder encoder;
	struct drm_connector connector;

	/* device info */
	unsigned int screen;
	unsigned int version;
	unsigned char id[8];
	struct mpro_info info;
	struct mpro_config config;

	unsigned char cmd[64];
};

static const uint32_t mpro_formats[] = {
	DRM_FORMAT_XRGB8888,
};

static const uint64_t mpro_primary_plane_format_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

static inline struct mpro_device *to_mpro(struct drm_device *dev) {
	return container_of(dev, struct mpro_device, dev);
}

static inline struct usb_device *mpro_to_usb_device(struct mpro_device *mpro) {
	return interface_to_usbdev(to_usb_interface(mpro -> dev.dev));
}

void drm_fb_xrgb8888_to_rgb565_flipped(struct iosys_map *dst, const unsigned int *dst_pitch,
				       const struct iosys_map *src, const struct drm_framebuffer *fb,
				       const struct drm_rect *clip, bool swab);

int mpro_mode(struct mpro_device *mpro);
int mpro_modeset(struct mpro_device* mpro);
int mpro_blit(struct mpro_device *mpro);

int mpro_init_planes(struct mpro_device *mpro);
int mpro_init_connector(struct mpro_device *mpro);
int mpro_init_sysfs(struct mpro_device *mpro);

int mpro_fbdev_setup(struct mpro_device *mpro, unsigned int preferred_bpp);

#endif /* _MPRO_H_ */

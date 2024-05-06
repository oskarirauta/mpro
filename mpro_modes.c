/* SPDX-License-Identifier: MIT */
#include <linux/usb.h>
#include <drm/drm_modes.h>
#include <drm/drm_print.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_rect.h>
#include "mpro.h"

#define MODEL_DEFAULT		"MPRO\n"
#define MODEL_5IN		"MPRO-5\n"
#define MODEL_5IN_OLED		"MPRO-5H\n"
#define MODEL_4IN3		"MPRO-4IN3\n"
#define MODEL_4IN		"MPRO-4\n"
#define MODEL_6IN8		"MPRO-6IN8\n"
#define MODEL_3IN4		"MPRO-3IN4\n"

static const char cmd_get_screen[5] = {
	0x51, 0x02, 0x04, 0x1f, 0xfc
};

static const char cmd_get_version[5] = {
	0x51, 0x02, 0x04, 0x1f, 0xf8
};

static const char cmd_get_id[5] = {
	0x51, 0x02, 0x08, 0x1f, 0xf0
};

static const struct drm_format_info *mpro_get_validated_format(struct drm_device *dev, const char *format_name) {

	static const struct mpro_format formats[] = MPRO_FORMATS;
	const struct mpro_format *fmt = formats;
	const struct mpro_format *end = fmt + ARRAY_SIZE(formats);
	const struct drm_format_info *info;

	if ( !format_name ) {
		drm_err(dev, "mpro: missing framebuffer format\n");
		return ERR_PTR(-EINVAL);
	}

	while ( fmt < end ) {
		if ( !strcmp(format_name, fmt->name)) {
			info = drm_format_info(fmt -> fourcc);
			if ( !info )
				return ERR_PTR(-EINVAL);
			return info;
		}
		++fmt;
	}

	drm_err(dev, "mpro: unknown framebuffer format %s\n", format_name);
	return ERR_PTR(-EINVAL);
}

static int mpro_get_screen(struct mpro_device *mpro) {

	struct usb_device *udev = mpro_to_usb_device(mpro);
	int ret;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
				0xb5, 0x40, 0, 0,
				(void*)cmd_get_screen, 5,
				MPRO_MAX_DELAY);
	if ( ret < 5 )
		return -EIO;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
				0xb6, 0xc0, 0, 0,
				mpro -> cmd, 1, MPRO_MAX_DELAY);
	if ( ret < 1 )
		return -EIO;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
				0xb7, 0xc0, 0, 0,
				mpro -> cmd, 5, MPRO_MAX_DELAY);
	if ( ret < 5 )
		return -EIO;

	mpro -> screen = ((unsigned int*)(mpro -> cmd + 1))[0];
	return 0;
}

static int mpro_get_version(struct mpro_device *mpro) {

	struct usb_device *udev = mpro_to_usb_device(mpro);
	int ret;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
				0xb5, 0x40, 0, 0,
				(void*)cmd_get_version, 5,
				MPRO_MAX_DELAY);
	if ( ret < 5 )
		return -EIO;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
				0xb6, 0xc0, 0, 0,
				mpro -> cmd, 1, MPRO_MAX_DELAY);
	if ( ret < 1 )
		return -EIO;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
				0xb7, 0xc0, 0, 0,
				mpro -> cmd, 5, MPRO_MAX_DELAY);
	if ( ret < 5 )
		return -EIO;

	mpro -> version = ((unsigned int *)(mpro -> cmd + 1))[0];
	return 0;
}

static int mpro_get_id(struct mpro_device *mpro) {

	struct usb_device *udev = mpro_to_usb_device(mpro);
	int ret;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
				0xb5, 0x40, 0, 0,
				(void*)cmd_get_id, 5,
				MPRO_MAX_DELAY);
	if ( ret < 5 )
		return -EIO;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
				0xb6, 0xc0, 0, 0,
				mpro -> cmd, 1, MPRO_MAX_DELAY);
	if ( ret < 1 )
		return -EIO;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
				0xb7, 0xc0, 0, 0,
				mpro -> cmd, 9, MPRO_MAX_DELAY);
	if ( ret < 5 )
		return -EIO;

	memcpy(mpro -> id, mpro -> cmd + 1, 8);
	return 0;
}

static void mpro_create_info(struct mpro_device *mpro, unsigned int width, unsigned int height,
			     unsigned int width_mm, unsigned int height_mm, unsigned int margin) {

	struct drm_rect rect = { .x1 = 0, .y1 = 0, .x2 = width - 1, .y2 = height - 1 };

	mpro -> info.width = width;
	mpro -> info.height = height;
	mpro -> info.width_mm = width_mm;
	mpro -> info.height_mm = height_mm;
	mpro -> info.margin = margin;
	mpro -> info.hz = 60;
	mpro -> info.rect = rect;
}

int mpro_mode(struct mpro_device *mpro) {

	struct drm_device *dev = &mpro -> dev;
	const struct drm_format_info *format;
	int stride, ret;

	ret = mpro_get_screen(mpro);
	if ( ret ) {
		drm_err(&mpro->dev, "can't get screen info.\n");
		return ret;
	}

	ret = mpro_get_version(mpro);
	if ( ret ) {
		drm_err(&mpro->dev, "can't get screen version.\n");
		return ret;
	}

	ret = mpro_get_id(mpro);
	if ( ret ) {
		drm_err(&mpro->dev, "can't get screen id.\n");
		return ret;
	}

	switch ( mpro -> screen ) {
	case 0x00000005:
		mpro -> info.model = MODEL_5IN;
		mpro_create_info(mpro, 480, 854, 62, 110, mpro -> version != 0x00000003 ? 320 : 0);
		break;
	case 0x00001005:
		mpro -> info.model = MODEL_5IN_OLED;
		mpro_create_info(mpro, 720, 1280, 62, 110, 0);
		break;
	case 0x00000304:
		mpro -> info.model = MODEL_4IN3;
		mpro_create_info(mpro, 480, 800, 56, 94, 0);
		break;
	case 0x00000004:
	case 0x00000b04:
	case 0x00000104:
		mpro -> info.model = MODEL_4IN;
		mpro_create_info(mpro, 480, 800, 53, 86, 0);
		break;
	case 0x00000007:
		mpro -> info.model = MODEL_6IN8;
		mpro_create_info(mpro, 800, 480, 89, 148, 0);
		break;
	case 0x00000403:
		mpro -> info.model = MODEL_3IN4;
		mpro_create_info(mpro, 800, 800, 88, 88, 0);
		break;
	default:
		mpro -> info.model = MODEL_DEFAULT;
		mpro_create_info(mpro, 480, 800, 0, 0, 0);
	}

	drm_info(&mpro -> dev, "VoCore Screen found, model: %s", mpro -> info.model);

	if ( mpro -> screen <= 2 ) {
		mpro -> config.partial = -1;
		drm_warn(&mpro -> dev, "device does not support partial frames");
	}

	const struct drm_display_mode mode = {
		DRM_MODE_INIT(mpro -> info.hz, mpro -> info.width, mpro -> info.height, mpro -> info.width_mm, mpro -> info.height_mm)
	};

	mpro -> mode = mode;

	format = mpro_get_validated_format(dev, "r5g6b5" /*"x8r8g8b8"*/);
	if ( IS_ERR(format))
		return -EINVAL;

	stride = drm_format_info_min_pitch(format, 0, mpro -> info.width);
	if ( drm_WARN_ON(dev, !stride))
		return -EINVAL;

	mpro -> info.stride = stride;
	mpro -> format = format;
	mpro -> pitch = stride;

	drm_dbg(dev, "display mode={" DRM_MODE_FMT "}\n", DRM_MODE_ARG(&mpro -> mode));
	drm_dbg(dev, "framebuffer format=%p4cc, size=%dx%d, stride=%d byte\n",
		&format -> format, mpro -> info.width, mpro -> info.height, stride);

	return 0;
}

static const struct drm_mode_config_funcs mpro_mode_config_funcs = {
	.fb_create = drm_gem_fb_create_with_dirty,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

int mpro_modeset(struct mpro_device* mpro) {

	struct drm_device *dev = &mpro -> dev;
	int ret;

	ret = drmm_mode_config_init(dev);
	if (ret)
		return ret;

	dev -> mode_config.min_width = mpro -> info.width;
	dev -> mode_config.max_width = mpro -> info.width;
	dev -> mode_config.min_height = mpro -> info.height;
	dev -> mode_config.max_height = mpro -> info.height;
	dev -> mode_config.preferred_depth = MPRO_BPP;
	dev -> mode_config.funcs = &mpro_mode_config_funcs;

	return 0;
}

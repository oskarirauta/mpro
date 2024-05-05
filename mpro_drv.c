/* SPDX-License-Identifier: MIT */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <drm/drm_atomic.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_format_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_print.h>
#include "mpro.h"

#define DRIVER_NAME	"mpro"
#define DRIVER_DESC	"DRM driver for VoCore Screen"
#define DRIVER_DATE	"20240505"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

static int partial = 0;
module_param(partial, int, 0660);
MODULE_PARM_DESC(partial, "set partial to 1 to enable partial screen updates");

static int flipx = 0;
module_param(flipx, int, 0660);
MODULE_PARM_DESC(flipx, "set flipx to 1 to flip image on x axis");

static int mpro_data_alloc(struct mpro_device *mpro) {

	unsigned int block_size = mpro -> info.height * mpro -> info.width * MPRO_BPP / 8 + mpro -> info.margin;

	mpro -> data = drmm_kmalloc(&mpro -> dev, block_size, GFP_KERNEL);
	if ( !mpro -> data )
		return -ENOMEM;

	mpro -> block_size = block_size;
	return 0;
}

static struct mpro_device *mpro_device_create(struct drm_driver *drv, struct usb_interface *interface) {

	struct mpro_device *mpro;
	struct drm_device *dev;
	int ret;

	mpro = devm_drm_dev_alloc(&interface -> dev, drv, struct mpro_device, dev);
	if ( IS_ERR(mpro))
		return ERR_CAST(mpro);
	dev = &mpro -> dev;

	/* Config */
	mpro -> config.flipx = flipx == 0 ? 0 : 1;
	mpro -> config.partial = partial == 0 ? 0 : 1;

	if ( mpro -> config.flipx )
		drm_info(dev, "image flip on x axis is enabled");

	/* Hardware setup */
	mpro -> dmadev = usb_intf_get_dma_device(to_usb_interface(dev -> dev));
	if ( !mpro -> dmadev )
		drm_warn(dev, "buffer sharing not supported"); /* not an error */

	ret = mpro_mode(mpro);
	if ( ret )
		return ERR_PTR(ret);

	/* Memory management */
	ret = mpro_data_alloc(mpro);
	if ( ret )
		return ERR_PTR(ret);

	iosys_map_set_vaddr(&mpro -> screen_base, mpro -> data);
	if ( iosys_map_is_null(&mpro -> screen_base)) {
		drm_err(dev, "failed to allocate buffer");
		return ERR_CAST(mpro);
	}

	/* Modesetting */
	ret = mpro_modeset(mpro);
	if ( ret )
		return ERR_PTR(ret);

	/* Primary plane */
	ret = mpro_init_planes(mpro);
	if (ret)
		return ERR_PTR(ret);

	/* CRTC, Encoder, Connector */
	ret = mpro_init_connector(mpro);
	if (ret)
		return ERR_PTR(ret);

	drm_mode_config_reset(dev);

	return mpro;
}

DEFINE_DRM_GEM_FOPS(mpro_fops);

static struct drm_driver mpro_driver = {
	DRM_GEM_SHMEM_DRIVER_OPS,
	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
	.driver_features	= DRIVER_ATOMIC | DRIVER_GEM | DRIVER_MODESET,
	.fops			= &mpro_fops,
};

static int mpro_probe(struct usb_interface* interface, const struct usb_device_id* id) {

	struct mpro_device *mpro;
	struct drm_device *dev;
	int ret;

	mpro = mpro_device_create(&mpro_driver, interface);
	if ( IS_ERR(mpro))
		return PTR_ERR(mpro);

	dev = &mpro -> dev;
	ret = drm_dev_register(dev, 0);
	if ( ret )
		return ret;

	ret = mpro_init_sysfs(mpro);
	if ( ret )
		drm_warn(dev, "failed to add sysfs entries");

	// prepare blit setup
	mpro_prepare(mpro);
	drm_fbdev_generic_setup(dev, 16);

	return 0;
}

static void mpro_remove(struct usb_interface *interface) {

	struct drm_device *dev = usb_get_intfdata(interface);
	struct mpro_device *mpro = to_mpro(dev);

	drm_dev_unplug(dev);
	drm_dev_unregister(dev);
	drm_atomic_helper_shutdown(dev);
	put_device(mpro -> dmadev);
	mpro -> dmadev = NULL;
}

static const struct usb_device_id mpro_of_match_table[] = {
	{ .match_flags = USB_DEVICE_ID_MATCH_DEVICE, .idVendor = 0xc872, .idProduct = 0x1004 },
	{ },
};
MODULE_DEVICE_TABLE(of, mpro_of_match_table);

static struct usb_driver mpro_usb_driver = {
	.name = "mpro",
	.probe = mpro_probe,
	.disconnect = mpro_remove,
	.id_table = mpro_of_match_table,
};

module_usb_driver(mpro_usb_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Oskari Rauta <oskari.rauta@gmail.com>");
MODULE_LICENSE("Dual MIT/GPL");

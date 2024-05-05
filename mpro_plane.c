/* SPDX-License-Identifier: MIT */
#include <drm/drm_atomic.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_print.h>
#include "mpro.h"

static void mpro_primary_plane_helper_atomic_update(struct drm_plane *plane, struct drm_atomic_state *state) {

	struct drm_plane_state *plane_state = drm_atomic_get_new_plane_state(state, plane);
	struct drm_plane_state *old_plane_state = drm_atomic_get_old_plane_state(state, plane);
	struct drm_shadow_plane_state *shadow_plane_state = to_drm_shadow_plane_state(plane_state);
	struct drm_framebuffer *fb = plane_state -> fb;
	struct drm_device *dev = plane -> dev;
	struct mpro_device *mpro = to_mpro(dev);
	struct drm_atomic_helper_damage_iter iter;
	struct drm_rect damage;
	int ret, idx;

	ret = drm_gem_fb_begin_cpu_access(fb, DMA_FROM_DEVICE);
	if (ret)
		return;

	if ( !drm_dev_enter(dev, &idx))
		goto out_drm_gem_fb_end_cpu_access;

	drm_atomic_helper_damage_iter_init(&iter, old_plane_state, plane_state);
	drm_atomic_for_each_plane_damage(&iter, &damage) {

		struct drm_rect dst_clip = plane_state -> dst;
		struct iosys_map dst = mpro -> screen_base;

		if (!drm_rect_intersect(&dst_clip, &damage))
			continue;

		iosys_map_incr(&dst, drm_fb_clip_offset(mpro -> pitch, mpro -> format, &dst_clip));

		if ( mpro -> config.flipx )
			drm_fb_xrgb8888_to_rgb565_flipped(&dst, &mpro -> pitch, shadow_plane_state -> data, fb, &damage, false);
		else
			drm_fb_xrgb8888_to_rgb565(&dst, &mpro -> pitch, shadow_plane_state -> data, fb, &damage, false);

		// update partial here, damage is the rect
	}

	// fullscreen only
	mpro_blit(mpro);

	drm_dev_exit(idx);

out_drm_gem_fb_end_cpu_access:
	drm_gem_fb_end_cpu_access(fb, DMA_FROM_DEVICE);
}

static void mpro_primary_plane_helper_atomic_disable(struct drm_plane *plane, struct drm_atomic_state *state) {

	struct drm_device *dev = plane -> dev;
	struct mpro_device *mpro = to_mpro(dev);
	int idx;

	if ( !drm_dev_enter(dev, &idx))
		return;

	/* Clear screen to black if disabled */
	iosys_map_memset(&mpro -> screen_base, 0, 0, mpro -> block_size);
	mpro_blit(mpro);

	drm_dev_exit(idx);
}

static const struct drm_plane_helper_funcs mpro_primary_plane_helper_funcs = {
	DRM_GEM_SHADOW_PLANE_HELPER_FUNCS,
	.atomic_check = drm_plane_helper_atomic_check,
	.atomic_update = mpro_primary_plane_helper_atomic_update,
	.atomic_disable = mpro_primary_plane_helper_atomic_disable,
};

static const struct drm_plane_funcs mpro_primary_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	DRM_GEM_SHADOW_PLANE_FUNCS,
};

int mpro_init_planes(struct mpro_device *mpro) {

	struct drm_device *dev = &mpro -> dev;
	struct drm_plane *primary_plane = &mpro -> primary_plane;
	const struct drm_format_info *format = mpro -> format;
	size_t nformats;
	int ret;

	nformats = drm_fb_build_fourcc_list(dev, &format -> format, 1, mpro -> formats, ARRAY_SIZE(mpro -> formats));

	ret = drm_universal_plane_init(dev, primary_plane, 0, &mpro_primary_plane_funcs,
				       mpro -> formats, nformats,
				       mpro_primary_plane_format_modifiers,
				       DRM_PLANE_TYPE_PRIMARY, NULL);
	if ( ret )
		return ret;

	drm_plane_helper_add(primary_plane, &mpro_primary_plane_helper_funcs);
	drm_plane_enable_fb_damage_clips(primary_plane);

	return 0;
}

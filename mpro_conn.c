/* SPDX-License-Identifier: MIT */
#include <drm/drm_atomic_helper.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_probe_helper.h>
#include "mpro.h"

static enum drm_mode_status mpro_crtc_helper_mode_valid(struct drm_crtc *crtc,
							     const struct drm_display_mode *mode) {
	struct mpro_device *mpro = to_mpro(crtc -> dev);
	return drm_crtc_helper_mode_valid_fixed(crtc, mode, &mpro -> mode);
}

static const struct drm_crtc_helper_funcs mpro_crtc_helper_funcs = {
	.mode_valid = mpro_crtc_helper_mode_valid,
	.atomic_check = drm_crtc_helper_atomic_check,
};

static const struct drm_crtc_funcs mpro_crtc_funcs = {
	.reset = drm_atomic_helper_crtc_reset,
	.destroy = drm_crtc_cleanup,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

static const struct drm_encoder_funcs mpro_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int mpro_connector_helper_get_modes(struct drm_connector *connector) {

	struct mpro_device *mpro = to_mpro(connector -> dev);
	return drm_connector_helper_get_modes_fixed(connector, &mpro -> mode);
}

static const struct drm_connector_helper_funcs mpro_connector_helper_funcs = {
	.get_modes = mpro_connector_helper_get_modes,
};

static const struct drm_connector_funcs mpro_connector_funcs = {
	.reset = drm_atomic_helper_connector_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

int mpro_init_connector(struct mpro_device* mpro) {

	struct drm_device *dev = &mpro -> dev;
	struct drm_crtc *crtc = &mpro -> crtc;
	struct drm_encoder *encoder = &mpro -> encoder;
	struct drm_connector *connector = &mpro -> connector;
	struct drm_plane *primary_plane = &mpro -> primary_plane;
	int ret;

	ret = drm_crtc_init_with_planes(dev, crtc, primary_plane, NULL,
					&mpro_crtc_funcs, NULL);
	if ( ret )
		return ret;

	drm_crtc_helper_add(crtc, &mpro_crtc_helper_funcs);

	/* Encoder */

	// DRM_MODE_ENCODER_NONE or DRM_MODE_ENCODER_VIRTUAL?
	ret = drm_encoder_init(dev, encoder, &mpro_encoder_funcs,
			       DRM_MODE_ENCODER_NONE, NULL);
	if ( ret )
		return ret;

	encoder -> possible_crtcs = drm_crtc_mask(crtc);
	//encoder -> possible_crtcs = 1;

	/* Connector */
	ret = drm_connector_init(dev, connector, &mpro_connector_funcs,
				 DRM_MODE_CONNECTOR_USB);
	if ( ret )
		return ret;

	drm_connector_helper_add(connector, &mpro_connector_helper_funcs);
	drm_connector_set_panel_orientation_with_quirk(connector,
						       DRM_MODE_PANEL_ORIENTATION_UNKNOWN,
						       mpro -> info.width, mpro -> info.height);

	return drm_connector_attach_encoder(connector, encoder);
}

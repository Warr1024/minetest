#ifndef _GUI_SCALING_FILTER_H_
#define _GUI_SCALING_FILTER_H_

#include "irrlichttypes_extrabloated.h"

/* Manually insert an image into the cache, useful to avoid texture-to-image
 * conversion whenever we can intercept it.
 */
void guiScalingCache(io::path key, video::IImage *value);

video::ITexture *guiScalingResizeCached(video::IVideoDriver *, video::ITexture *,
		const core::rect<s32> &, const core::rect<s32> &);

video::ITexture *guiScalingImageButton(video::IVideoDriver *driver, video::ITexture *src,
	s32 width, s32 height);

void draw2DImageFilterScaled(video::IVideoDriver *driver, video::ITexture *txr,
		const core::rect<s32> &destrect, const core::rect<s32> &srcrect,
		const core::rect<s32> *cliprect, const video::SColor *const colors,
		bool usealpha);

#endif

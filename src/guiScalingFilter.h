#ifndef _GUI_SCALING_FILTER_H_
#define _GUI_SCALING_FILTER_H_

#include "irrlichttypes_extrabloated.h"

video::ITexture *guiScalingResizeCached(video::IVideoDriver *, video::ITexture *,
		const core::rect<s32> &, const core::rect<s32> &);

core::rect<s32> guiScalingSourceRect(const core::rect<s32> &, const core::rect<s32> &);

void draw2DImageFilterScaled(video::IVideoDriver *, video::ITexture *, const core::rect<s32> &,
		const core::rect<s32> &, const core::rect<s32> *, const video::SColor *, bool);

#endif

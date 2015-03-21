#include "guiscalingfilter.h"
#include "imagefilters.h"
#include "settings.h"
#include "main.h"		// for g_settings
#include <stdio.h>

/* Maintain a static cache to store the images that correspond to textures
 * in a format that's manipulable by code.  Some platforms exhibit issues
 * converting textures back into images repeatedly, and some don't even
 * allow it at all.
 */
std::map<io::path, video::IImage *> imgCache;

/* Manually insert an image into the cache, useful to avoid texture-to-image
 * conversion whenever we can intercept it.
 */
void guiScalingCache(io::path key, video::IImage *value) {
	if (!g_settings->getBool("gui_scaling_filter"))
		return;
	value->grab();
	imgCache[key] = value;
}

core::rect<s32> guiScalingSourceRect(const core::rect<s32> &srcrect,
		const core::rect<s32> &destrect) {
	return g_settings->getBool("gui_scaling_filter")
		? core::rect<s32>(0, 0, destrect.getWidth(), destrect.getHeight())
		: srcrect;
}

u32 nextpower2(u32 orig) {
	orig--;
	orig |= orig >> 1;
	orig |= orig >> 2;
	orig |= orig >> 4;
	orig |= orig >> 8;
	orig |= orig >> 16;
	orig++;
}

video::ITexture *guiScalingResizeCached(video::IVideoDriver *driver, video::ITexture *src,
		const core::rect<s32> &srcrect, const core::rect<s32> &destrect) {

	if (!g_settings->getBool("gui_scaling_filter"))
		return src;

	// Calculate scaled texture name.
	char rectstr[200];
	sprintf(rectstr, "%d:%d:%d:%d:%d:%d",
		srcrect.UpperLeftCorner.X,
		srcrect.UpperLeftCorner.Y,
		srcrect.getWidth(),
		srcrect.getHeight(),
		destrect.getWidth(),
		destrect.getHeight());
	io::path origname = src->getName().getPath();
	io::path scalename = origname + "@guiScalingFilter:" + rectstr;

	// Search for existing scaled texture.
	video::ITexture *scaled = driver->getTexture(scalename);
	if(scaled)
		return scaled;

	// Try to find the texture converted to an image in the cache.
	// If the image was not found, try to extract it from the texture.
	video::IImage* srcimg = imgCache[origname];
	if (srcimg == NULL) {
		srcimg = driver->createImageFromData(src->getColorFormat(),
			src->getSize(), src->lock(), false);
		src->unlock();
		imgCache[origname] = srcimg;
	}

	// Create a new destination image and scale the source into it.
	imageCleanTransparent(srcimg, 0);
	video::IImage *destimg = driver->createImage(src->getColorFormat(),
			core::dimension2d<u32>((u32)destrect.getWidth(),
			(u32)destrect.getHeight()));
	imageScaleNNAA(srcimg, srcrect, destimg);

	// Android is very picky about textures being powers of 2, so expand
	// the image dimensions to the next power of 2, if necessary, for
	// that platform.
#ifdef __ANDROID__
	video::IImage *po2img = driver->createImage(src->getColorFormat(),
			core::dimension2d<u32>(nextpower2((u32)destrect.getWidth()),
			nextpower2((u32)destrect.getHeight())));
	destimg->copyTo(po2img);
	destimg->drop();
	destimg = po2img;
#endif

	// Convert the scaled image back into a texture.
	scaled = driver->addTexture(scalename, destimg, NULL);
	destimg->drop();

	return scaled;
}

video::ITexture *guiScalingImageButton(video::IVideoDriver *driver, video::ITexture *src,
		s32 width, s32 height) {
	return guiScalingResizeCached(driver, src,
		core::rect<s32>(0, 0, src->getSize().Width, src->getSize().Height),
		core::rect<s32>(0, 0, width, height));
}

void draw2DImageFilterScaled(video::IVideoDriver *driver, video::ITexture *txr,
		const core::rect<s32> &destrect, const core::rect<s32> &srcrect,
		const core::rect<s32> *cliprect = 0, const video::SColor *const colors = 0,
		bool usealpha = false) {
	driver->draw2DImage(
			guiScalingResizeCached(driver, txr, srcrect, destrect),
			destrect,
			guiScalingSourceRect(srcrect, destrect),
			cliprect, colors, usealpha || g_settings->getBool("gui_scaling_filter"));
}

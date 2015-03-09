#include "guiScalingFilter.h"
#include "settings.h"
#include "main.h"		// for g_settings
#include <stdio.h>

// Maintain a static cache to store images converted from textures.
// Some platforms seem to have an issue with re-converting the same
// texture to image repeatedly, besides the efficiency issues.
std::map<io::path, video::IImage *> imgCache;

/* Fill in RGB values for transparent pixels, since we need to be able to blend
 * to them, but PNG optimizers throw this information away.
 */
void guiScalingFillTransparent(video::IImage *src) {

	core::dimension2d<u32> dim = src->getDimension();

	// Walk each pixel looking for fully transparent ones.
	for (u32 ctrx = 0; ctrx < dim.Width; ctrx++)
		for (u32 ctry = 0; ctry < dim.Height; ctry++) {
			irr::video::SColor c = src->getPixel(ctrx, ctry);
			if (c.getAlpha() > 0)
				continue;

			// Sample size and total weighted r, g, b values.
			u32 ss = 0, sr = 0, sg = 0, sb = 0;

			// Walk each neighbor pixel (clipped to image bounds).
			for (u32 sx = (ctrx < 1) ? 0 : (ctrx - 1);
					sx <= (ctrx + 1) && sx < dim.Width; sx++)
				for (u32 sy = (ctry < 1) ? 0 : (ctry - 1);
						sy <= (ctry + 1) && sy < dim.Height; sy++) {

					// Ignore the center pixel (its RGB is already
					// presumed meaningless).
					if ((sx == ctrx) && (sy == ctry))
						continue;

					// Add RGB values weighted by alpha.
					irr::video::SColor d = src->getPixel(sx, sy);
					u32 a = d.getAlpha();
					ss += a;
					sr += a * d.getRed();
					sg += a * d.getGreen();
					sb += a * d.getBlue();
				}

			// If we found any neighbor RGB data, set pixel to average
			// weighted by alpha.
			if (ss > 0) {
				c.setRed(sr / ss);
				c.setGreen(sg / ss);
				c.setBlue(sb / ss);
				src->setPixel(ctrx, ctry, c);
			}
		}
}

/* Scale a region of an image into another image, using nearest-neighbor with
 * anti-aliasing; treat pixels as crisp rectangles, but blend them at boundaries
 * to prevent non-integer scaling ratio artifacts.
 */
void guiScalingResize(video::IImage *src, const core::rect<s32> &srcrect,
		video::IImage *dest, const core::rect<s32> &destrect) {

	double minsx, maxsx, minsy, maxsy, area, ra, ga, ba, aa, pw, ph, pa, tmp;
	s32 dy, dx, sy, sx, maxs32sx, maxs32sy;
	video::SColor pxl;

	guiScalingFillTransparent(src);

	// Locals for destination and source origin, width, and height.
	double dox = destrect.UpperLeftCorner.X * 1.0;
	double doy = destrect.UpperLeftCorner.Y * 1.0;
	s32 dw = destrect.getWidth();
	s32 dh = destrect.getHeight();
	double sox = srcrect.UpperLeftCorner.X * 1.0;
	double soy = srcrect.UpperLeftCorner.Y * 1.0;
	double sw = srcrect.getWidth() * 1.0;
	double sh = srcrect.getHeight() * 1.0;

	// Loop over Y, then X, assuming that image is stored in raster
	// order (horizontal scans) so X-ordering should give us better cache
	// locality.
	for (dy = 0; dy < dh; dy++)
		for (dx = 0; dx < dw; dx++) {

			// Calculate floating-point source rectangle bounds.
			// Do some basic clipping, and for mirrored/flipped rects,
			// make sure min/max are in the right order.
			minsx = sox + (dx * sw / dw);
			if (minsx < 0)
				minsx = 0;
			else if (minsx > sw)
				minsx = sw;
			maxsx = minsx + sw / dw;
			if (maxsx < 0)
				maxsx = 0;
			else if (maxsx > sw)
				maxsx = sw;
			if (minsx > maxsx) {
				tmp = minsx;
				minsx = maxsx;
				maxsx = tmp;
			}
			minsy = soy + (dy * sh / dh);
			if (minsy < 0)
				minsy = 0;
			else if (minsy > sh)
				minsy = sh;
			maxsy = minsy + sh / dh;
			if (maxsy < 0)
				maxsy = 0;
			else if (maxsy > sh)
				maxsy = sh;
			if (minsy > maxsy) {
				tmp = minsy;
				minsy = maxsy;
				maxsy = tmp;
			}

			// Total area, and integral of r, g, b values over that area,
			// initialized to zero, to be summed up in next loops.
			area = 0;
			ra = 0;
			ga = 0;
			ba = 0;
			aa = 0;

			// Loop over the integral pixel positions described by those bounds.
			maxs32sx = (s32)maxsx;
			maxs32sy = (s32)maxsy;
			for (sy = (s32)minsy; sy <= maxs32sy; sy++)
				for (sx = (s32)minsx; sx <= maxs32sx; sx++) {

					// Calculate width, height, then area of dest pixel
					// that's covered by this source pixel.
					pw = 1;
					if (minsx > sx)
						pw += sx - minsx;
					if (maxsx < (sx + 1))
						pw += maxsx - sx - 1;
					ph = 1;
					if (minsy > sy)
						ph += sy - minsy;
					if (maxsy < (sy + 1))
						ph += maxsy - sy - 1;
					pa = pw * ph;

					// Get source pixel and add it to totals, weighted
					// by covered area and alpha.
					pxl = src->getPixel(sx, sy);
					area += pa;
					ra += pa * pxl.getRed();
					ga += pa * pxl.getGreen();
					ba += pa * pxl.getBlue();
					aa += pa * pxl.getAlpha();
				}

			// Set the destination image pixel to the average color.
			if (area > 0) {
				pxl.setRed(ra / area + 0.5);
				pxl.setGreen(ga / area + 0.5);
				pxl.setBlue(ba / area + 0.5);
				pxl.setAlpha(aa / area + 0.5);
			} else {
				pxl.setRed(0);
				pxl.setGreen(0);
				pxl.setBlue(0);
				pxl.setAlpha(0);
			}
			dest->setPixel(dox + dx, doy + dy, pxl);
		}
}

bool guiScalingEnabled() {
	return g_settings->exists("gui_scale_filter") && g_settings->getBool("gui_scale_filter");
}

core::rect<s32> guiScalingSourceRect(const core::rect<s32> &srcrect, const core::rect<s32> &destrect) {
	if(!guiScalingEnabled())
		return srcrect;
	return core::rect<s32>(0, 0, destrect.getWidth(), destrect.getHeight());
}

video::ITexture *guiScalingResizeCached(video::IVideoDriver *driver, video::ITexture *src,
		const core::rect<s32> &srcrect, const core::rect<s32> &destrect) {

	// Allow filter to be disabled.
	if (!guiScalingEnabled())
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
	video::IImage* srcimg = imgCache[origname];

	// Convert the texture back into an image.
	if (srcimg == NULL) {
		srcimg = driver->createImageFromData(src->getColorFormat(),
			src->getSize(), src->lock(), false);
		src->unlock();

		// Store converted image in the cache.
		imgCache[origname] = srcimg;
	}

	// Create a new destination image and scale the source into it.
	video::IImage *destimg = driver->createImage(src->getColorFormat(),
		core::dimension2d<u32>((u32)destrect.LowerRightCorner.X, (u32)destrect.LowerRightCorner.Y));
	guiScalingResize(srcimg, srcrect, destimg, core::rect<s32>(0, 0, destrect.getWidth(), destrect.getHeight()));

	// Convert the scaled image back into a texture.
	scaled = driver->addTexture(scalename, destimg, NULL);
	destimg->drop();

	return scaled;
}

void draw2DImageFilterScaled(video::IVideoDriver *driver, video::ITexture *txr,
		const core::rect<s32> &destrect, const core::rect<s32> &srcrect,
		const core::rect<s32> *cliprect = 0, const video::SColor *const colors = 0,
		bool usealpha = false) {
	driver->draw2DImage(
		guiScalingResizeCached(driver, txr, srcrect, destrect),
		destrect,
		guiScalingSourceRect(srcrect, destrect),
		cliprect, colors, usealpha || guiScalingEnabled());
}

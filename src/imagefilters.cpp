#include "imagefilters.h"

/* Fill in RGB values for transparent pixels, to correct for odd colors
 * appearing at borders when blending.  This is because many PNG optimizers
 * like to discard RGB values of transparent pixels, but when blending then
 * with non-transparent neighbors, their RGB values will shpw up nonetheless.
 *
 * This function modifies the original image in-place.
 *
 * Parameter "threshold" is the alpha level below which pixels are considered
 * transparent.  Should be 127 for 3d where alpha is threshold, but 0 for
 * 2d where alpha is blended.
 */
void imageCleanTransparent(video::IImage *src, u32 threshold) {

	core::dimension2d<u32> dim = src->getDimension();

	// Walk each pixel looking for fully transparent ones.
	// Note: loop y around x for better cache locality.
	for (u32 ctry = 0; ctry < dim.Height; ctry++)
	for (u32 ctrx = 0; ctrx < dim.Width; ctrx++) {

		// Ignore opaque pixels.
		irr::video::SColor c = src->getPixel(ctrx, ctry);
		if (c.getAlpha() > threshold)
			continue;

		// Sample size and total weighted r, g, b values.
		u32 ss = 0, sr = 0, sg = 0, sb = 0;

		// Walk each neighbor pixel (clipped to image bounds).
		for (u32 sy = (ctry < 1) ? 0 : (ctry - 1);
				sy <= (ctry + 1) && sy < dim.Height; sy++)
		for (u32 sx = (ctrx < 1) ? 0 : (ctrx - 1);
				sx <= (ctrx + 1) && sx < dim.Width; sx++) {

			// Ignore transparent pixels.
			irr::video::SColor d = src->getPixel(sx, sy);
			if (d.getAlpha() <= threshold)
				continue;

			// Add RGB values weighted by alpha.
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
 * to prevent non-integer scaling ratio artifacts.  Note that this may cause
 * some blending at the edges where pixels don't line up perfectly, but this
 * filter is designed to produce the most accurate results for both upscaling
 * and downscaling.
 */
void imageScaleNNAA(video::IImage *src, const core::rect<s32> &srcrect, video::IImage *dest) {

	double minsx, maxsx, minsy, maxsy, area, ra, ga, ba, aa, pw, ph, pa, tmp;
	u32 dy, dx;
	s32 sy, sx, maxs32sx, maxs32sy;
	video::SColor pxl;

	// Cache rectsngle boundaries.
	double sox = srcrect.UpperLeftCorner.X * 1.0;
	double soy = srcrect.UpperLeftCorner.Y * 1.0;
	double sw = srcrect.getWidth() * 1.0;
	double sh = srcrect.getHeight() * 1.0;

	// Walk each destination image pixel.
	// Note: loop y around x for better cache locality.
	core::dimension2d<u32> dim = dest->getDimension();
	for (dy = 0; dy < dim.Height; dy++)
	for (dx = 0; dx < dim.Width; dx++) {

		// Calculate floating-point source rectangle bounds.
		// Do some basic clipping, and for mirrored/flipped rects,
		// make sure min/max are in the right order.
		minsx = sox + (dx * sw / dim.Width);
		if (minsx < 0)
			minsx = 0;
		else if (minsx > sw)
			minsx = sw;
		maxsx = minsx + sw / dim.Width;
		if (maxsx < 0)
			maxsx = 0;
		else if (maxsx > sw)
			maxsx = sw;
		if (minsx > maxsx) {
			tmp = minsx;
			minsx = maxsx;
			maxsx = tmp;
		}
		minsy = soy + (dy * sh / dim.Height);
		if (minsy < 0)
			minsy = 0;
		else if (minsy > sh)
			minsy = sh;
		maxsy = minsy + sh / dim.Height;
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
		dest->setPixel(dx, dy, pxl);
	}
}

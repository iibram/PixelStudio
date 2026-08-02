#include "ImageProcessor.hpp"


/*
	An excution example:
	The same image is loaded twice and are saved in separate locations (to not override themselves).
	img[0] is undergoing in a cascade 2 times a contrast manipulation with k = 2 and k = 1.25, that means a total contrast of 2 * 1.25 -> k = 2.5.
	img[1] is undergoing a single contrast manipulation with k = 2.5. So the RGBA values of the images (same source) must be identical after manipulations.

	Tests shown: Even +/- 0.000001 accuracy is not given in a cascade (a few pixels begin to differ).
	So, in cascades +/- 0.0 precision is essential for 100% correctness and this app is working at a very high precision and mathematically conform.^^

	Conclusion: Using floats in image processing is the most genius idea. No need for a 1.e-15 precision with double type (saving 4 Bytes / channel ).^^
*/
int main()
{
	std::string src = "../resources/IN/A_4K.png";

	ImageProcessor ip;

	// img[0], load and contrast(2)
	ip.loadImage(src.c_str());					// <- same source image
	ip.setContrast(2.0f);
	ip.saveImage();								// DEFAULT_OUT_PATH

	// img[1], load and contrast(2.5)
	ip.loadImage(src.c_str());					// <- same source image
	ip.setContrast(2.5f);						// even 2.500001f or 2.499999f blows it up
	ip.saveImage("../resources/OUT2");			// user selected location to save the image

	// select img[0] and 2nd contrast(1.25) -> 2.0 * 1.25 = 2.5
	if (ip.selectImage(0))
	{
		ip.setContrast(1.25f); 					// even 1.249999f or 1.250001f blows it up
		ip.saveImage();							// DEFAULT_OUT_PATH
	}

	// comparing img[0] with img[1]. Number of different pixels should be 0!
	ip.compareRGBA(1);


	// --------------------------------------------------------------------------------------
	// 						  QUICK TESTS DURING SOFTWARE DEVELOPEMENT
	// --------------------------------------------------------------------------------------

	// ip.loadImage(src.c_str());
	// ip.comicify(7);
	// // ip.scaleIntensity(0.45f);
	// // ip.applyHistogramEqualization();
	// // ip.applyAutoSegmentation(Segm_t::BLACK_WHITE | Segm_t::MAKE_BACKGROUND_TRANSPARENT);
	// ip.saveImage();

	return 0;
}

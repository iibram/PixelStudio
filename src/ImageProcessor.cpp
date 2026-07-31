#include "ImageProcessor.hpp"
#include <iostream>
#include <string>
#include <format>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "external/stb_image.h"
#include "external/stb_image_write.h"



// =============================================================  P U B L I C  =============================================================

/**
 * @brief Custom constructor: Creates the default output path if it does not exist, reports success and reserves a massive capacity
 * (16K UHD images) for the `tempRGBA` vector - essential for both displaying the image on screen and saving the image.
 */
ImageProcessor::ImageProcessor() : IDX(0)
{
	if (std::filesystem::create_directories(DEFAULT_OUT_PATH))
		std::cout << "successfully created the default output directory!\n";

	// pre-resizing the `tempRGBA` vector to a massive capacity for 16K UHD (15360 x 8640) resolution RGBA images
	tempRGBA.resize((15360ULL * 8640ULL << 2));
}

/**
 * @brief Loads the image file from the specified path and reports the success status. When successfully, the RGBA of the image are stored
 * at `tempRGBA`, are converted to the `Pixel` structure, and the image is pushed to the `images` vector.
 * @param img_path the image file to be loaded
 */
void ImageProcessor::loadImage(const char* img_path)
{
	if (img_path)
	{
		// loading the image from file (+ timing)
		t_start = clock::now();
		uint8_t* stb_data = stbi_load(img_path, &WIDTH, &HEIGHT, &CHANNELS, 4);			// getting the image RGBA data
		t_end = clock::now();

		if (!stb_data)
			std::cerr << std::format("\nloading image <- \"{}\": -- FAILED! --\n", img_path);

		else
		{
			std::cout << std::format("\nloading image <- \"{}\", {} x {}: -- SUCCESS! --\n", img_path, WIDTH, HEIGHT);
			showDuration("load image");

			uint64_t SIZE = (static_cast<uint64_t>(WIDTH) * HEIGHT) << 2;				// SIZE = data of stb_data -> (r,g,b,a,r,g,b,a...)
			tempRGBA.resize(SIZE);

			// --------------------------------
			// 		tempRGBA <- stb_data
			// --------------------------------
			t_start = clock::now();
			for (uint64_t i = 0; i < SIZE; ++i)
				tempRGBA[i] = stb_data[i];

			t_end = clock::now();
			showDuration("_tempRGBA_");

			stbi_image_free(stb_data);													// free(stb_data)

			// --------------------------------
			// 	   images[IDX] <- YUV data
			// --------------------------------
			images.push_back(Image());
			IDX = images.size() - 1;

			images[IDX].path = img_path;
			images[IDX].pxls.resize(SIZE >> 2);
			images[IDX].w = WIDTH;
			images[IDX].h = HEIGHT;

			std::cout << std::format("> sel image IDX: {:5}\n", IDX);

			toYUV();
		}
	}
}

/**
 * @brief Saves the currently loaded image as a PNG file to the provided (or default) path and reports the success status.
 * @note Calling `toRGBA()` completes the image processing, converts the data back to the original data structure (RGBA), and enables the call to
 * `stbi_write_png(..)` to save the PNG file.
 * @param out_path the custom filepath to save the image or `nullptr` if not explicitly passed
 */
void ImageProcessor::saveImage(const char* out_path)
{
	std::string finalPath;

	//std::string finalPath = (out_path ? out_path / images[IDX].path.filename() : DEFAULT_OUT_PATH / images[IDX].path.filename()).generic_string();

	if (out_path)
	{
		if (std::filesystem::create_directories(out_path))
			std::cout << "successfully created the custom output directory!\n";

		finalPath = (out_path / images[IDX].path.filename()).generic_string();
	}
	else
		finalPath = (DEFAULT_OUT_PATH / images[IDX].path.filename()).generic_string();

	// writing the image to file (+ timing)
	t_start = clock::now();
	int success = stbi_write_png(finalPath.c_str(), images[IDX].w, images[IDX].h, 4, tempRGBA.data(), (images[IDX].w << 2));
	t_end = clock::now();
	showDuration("save image");

	if (!success)
		std::cerr << std::format("saving image -> \"{}\": -- FAILED! --\n", finalPath);

	else
		std::cout << std::format("saving image -> \"{}\", {} x {}: -- SUCCESS! --\n", finalPath, images[IDX].w, images[IDX].h);
}

/**
 * @brief Switches the context to the image referenced by the provided index (if applicable) and immediately converts it to RGBA format.
 * @param idx the index to switch to
 */
bool ImageProcessor::selectImage(uint16_t idx)
{
	if (idx < images.size())
	{
		IDX = idx;
		std::cout << std::format("\n> sel image IDX: {:5}\n", IDX);
		toRGBA();
		return true;
	}
	return false;
}

/**
 * @brief Manipulates the overall intensity (Y [0, 1]) of the selected image by adding the passed value [-1, 1]
 * @param val the floating point value [-1, 1] to manipute the intesity by addition
 */
void ImageProcessor::addIntensity(float value)
{
	if (value != 0.0f && value >= -1.0f && value <= 1.0f)
	{
		t_start = clock::now();

		for (Pixel& p : images[IDX].pxls)
		{
			if (value > 0.0f)
				p.Y = fmin(p.Y + value, 1.0f);
			else
				p.Y = fmax(p.Y + value, 0.0f);
		}
		t_end = clock::now();

		showDuration("added Y(I)");
		toRGBA();
	}
	else
		std::cout << "intensity manip. skipped! (value must be within [-1, 1] \\{0})\n";
}

/**
 * @brief Manipulates the overall intensity (Y [0, 1]) of the selected image by percentually scaling the passed factor [-1, 1]
 * @param factor the floating point factor [-1, 1] to manipute the intesity by scaling
 */
void ImageProcessor::scaleIntensity(float factor)
{
	if (factor != 0.0f && factor >= -1.0f && factor <= 1.0f)
	{
		t_start = clock::now();
		float Y;

		for (Pixel& p : images[IDX].pxls)
		{
			Y = p.Y + (p.Y * factor);
			p.Y = Y > 1.0f ? 1.0f : Y;
		}
		t_end = clock::now();

		showDuration("scale Y(I)");
		toRGBA();
	}
	else
		std::cout << "intensity manip. skipped! (value must be within [-1, 1] \\{0})\n";
}

/**
 * @brief Manipulates the overall contrast of the selected image.
 * @param k the factor to manipulate the contrast [-1, 5]
 */
void ImageProcessor::setContrast(float k)
{
	if (k == 1.0f) std::cout << "contrast manip. skipped! (k = 1 changes nothing)\n";

	else if (k == -1.0f) toNegative();

	else if (k == 0.0f)	// seting each Y = 0.5f
	{
		t_start = clock::now();

		for (Pixel& p : images[IDX].pxls)
			p.Y = 0.5f;

		t_end = clock::now();

		showDuration("m contrast");
		toRGBA();
	}

	// for any other legit factor
	else if (k > 0.0f && k <= 5.0f)
	{
		t_start = clock::now();
		float Y;
		float factor = (k - 1.0f) / 2.0f;

		for (Pixel& p : images[IDX].pxls)
		{
			Y = k * p.Y - factor;
			p.Y = fmaxf(0.0f, fminf(Y, 1.0f));
		}
		t_end = clock::now();

		showDuration("m contrast");
		toRGBA();
	}
	else
		std::cout << "contrast manip. skipped! (value must be within [-1, 5])\n";
}

/**
 * @brief Inverts each Y value so the image become the true negative.
 * @note Also called by `setContrast(k)`, when k = -1.0f.
 */
void ImageProcessor::toNegative()
{
	t_start = clock::now();

	for (Pixel& p : images[IDX].pxls)
		p.Y = 1.0f - p.Y;

	t_end = clock::now();

	showDuration("m negative");
	toRGBA();
}

/**
 * @brief Compares the currently selected image with the image referenced by the specified index. Counts the differing pixels (in RGBA) if applicable
 * and outputs the information. Reports any issues, specifying the cause.
 * @param oIdx the index of the other image to compare the pixels
 */
void ImageProcessor::printDiffInRGBA(uint16_t oIdx)
{
	if (oIdx < images.size())
	{
		if (images[IDX].pxls.size() == images[oIdx].pxls.size())
		{
			t_start = clock::now();

			uint64_t diff = 0;
			size_t k = 0;

			for (Pixel& p : images[oIdx].pxls)
			{
				float R = p.Y + (p.V * INV_V_max);
				float B = p.Y + (p.U * INV_U_max);
				float G = (p.Y - k_R * R - k_B * B) * INV_k_G;

				if (tempRGBA[k + 0] != quantize(R) || tempRGBA[k + 1] != quantize(G) || tempRGBA[k + 2] != quantize(B) || tempRGBA[k + 3] != quantize(p.A))
				{
					++diff;
				}
				k += 4;
			}
			t_end = clock::now();
			showDuration(std::format("comp {:02}:{:02}", IDX, oIdx).c_str());

			std::cout << std::format("num of \u0394 pixels: {:9}\n", diff);
		}
		else
			std::cout << "compare images skipped! (referred image has a different size)\n";
	}
	else
		std::cout << "compare images skipped! (referred image is not available)\n";
}


// ============================================================  P R I V A T E  ============================================================

/**
 * @brief Converts the current `tempRGBA` data into the `Pixel` data structure (YUV + A). During conversion, the alpha channel values ​​are
 * checked for variations. Since the pixels in most images share the same alpha value, it makes sense to precalculate the conversion once
 * and apply this value to every pixel. The key aspect of this approach is the significantly more efficient reuse of `toRGBA()`, which is
 * called after every image processing step or image selection to display the result immediately on the screen.
 */
void ImageProcessor::toYUV()
{
	t_start = clock::now();

	uint8_t uA0 = tempRGBA[3];
	float fA0 = static_cast<float>(uA0) * INV_255;
	bool stableAlpha = true;
	uint64_t k = 0;

	for (Pixel& p : images[IDX].pxls)
	{
		float R = static_cast<float>(tempRGBA[k + 0]) * INV_255;
		float G = static_cast<float>(tempRGBA[k + 1]) * INV_255;
		float B = static_cast<float>(tempRGBA[k + 2]) * INV_255;

		uint8_t currAlpha = tempRGBA[k + 3];

		if (currAlpha == uA0) p.A = fA0;
		else
		{
			p.A = static_cast<float>(currAlpha) * INV_255;
			stableAlpha = false;
		}

		p.Y = k_R * R + k_G * G + k_B * B;
		p.U = k_U * (B - p.Y) * INV_k_B;
		p.V = k_V * (R - p.Y) * INV_k_R;

		k += 4;
	}
	images[IDX].stableAlpha = stableAlpha;
	t_end = clock::now();

	showDuration("RGBA > YUV");
}

/**
 * @brief Reconstructs the RGBA values by the `Pixel` (YUV + A) type data of the selected image and stores these channel by channel at `tempRGBA`.
 */
void ImageProcessor::toRGBA()
{
	t_start = clock::now();
	tempRGBA.resize((images[IDX].pxls.size() << 2));

	bool stable = images[IDX].stableAlpha;
	uint8_t quantAlpha = stable ? quantize(images[IDX].pxls[0].A) : 0;
	uint64_t k = 0;

	for (Pixel& p : images[IDX].pxls)
	{
		float R = p.Y + (p.V * INV_V_max);
		float B = p.Y + (p.U * INV_U_max);
		float G = (p.Y - k_R * R - k_B * B) * INV_k_G;

		tempRGBA[k + 0] = quantize(R);
		tempRGBA[k + 1] = quantize(G);
		tempRGBA[k + 2] = quantize(B);
		tempRGBA[k + 3] = stable ? quantAlpha : quantize(p.A);

		k += 4;
	}
	t_end = clock::now();

	showDuration("RGBA < YUV");
}

/**
 * @brief Quantizes the given `float` value in the interval [0, 1] and returns it as a commercially rounded `uint8_t` value in the range [0 – 255].
 * @param f the `float` number to be quantized (I: [0, 1])
 * @return the quantized `uint8_t` number (I: [0, 255])
 */
uint8_t ImageProcessor::quantize(float f)
{
	f = fmaxf(0.0f, fminf(f, 1.0f));				// clamping the range [0, 1] (fminf & fmaxf are intrinsics and super fast)
	f = f * 255.0f + 0.5f;							// scaling up to [0, 255] and round commercially (step 1)

	return static_cast<uint8_t>(f);					// this cast finalizes the commercialy rounding (step 2)
}

/**
 * @brief Evaluates and outputs the time required for the most recently performed image processing operation.
 * @param text a brief explanatory text regarding the type of manipulation performed
 */
void ImageProcessor::showDuration(const char* text)
{
	dur = t_end - t_start;
	std::cout << std::format("\u0394t ({}): {:9.3f} ms\n", text, dur.count()); // \u0394 = Δ
}

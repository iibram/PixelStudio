#pragma once

#include <filesystem>
#include <vector>
#include <chrono>



enum Segm_t : uint8_t { COLORED, BLACK_WHITE, MAKE_BACKGROUND_TRANSPARENT };

/**
 * @brief Provides a professional and efficient environment for image processing.
 * @note Uses externally the "stb" library by [Sean Barrett](https://github.com/nothings/stb) exclusively for reading and writing PNG files.
 */
class ImageProcessor
{
public:
	ImageProcessor();
	~ImageProcessor() = default;

	void loadImage(const char* img_path);
	void saveImage(const char* out_path = nullptr);

	bool selectImage(uint8_t idx);

	void addIntensity(float value);
	void scaleIntensity(float factor);
	void setContrast(float k);
	void toNegative();

	void applyHistogramEqualization();
	void applySegmentation(float t, uint8_t segmType = Segm_t::BLACK_WHITE);
	void applyAutoSegmentation(uint8_t segmType = Segm_t::BLACK_WHITE);
	void comicify(uint8_t exp);

	void compareRGBA(uint8_t oIdx);
	float getBHT();

private:
	using clock = std::chrono::steady_clock;
	using time_point = clock::time_point;
	using duration = std::chrono::duration<double, std::milli>;
	using path = std::filesystem::path;

	inline static const path DEFAULT_OUT_PATH = "../resources/OUT";	  // default filepath for image outputs

	//enum ImageType : uint8_t { PNG, JPG, BMP };

	// ========================================================================================================
	// 									 Original YUV constants (YCbCr Model)
	// ========================================================================================================
	constexpr static float	 k_R = 0.299f;		// YUV constant for the RGB R-channel
	constexpr static float	 k_G = 0.587f;		// YUV constant for the RGB G-channel. k_G = 1 - (k_R + k_B)
	constexpr static float	 k_B = 0.114f;		// YUV constant for the RGB B-channel

	constexpr static float	 k_U = 0.500f;		// YUV constant for the color difference (blue, horizontal)
	constexpr static float 	 k_V = 0.500f;		// YUV constant for the color difference (red, vertical)

	constexpr static float U_max = 0.564f;		// YUV constant for the max. value of U (horizontal)
	constexpr static float V_max = 0.713f;		// YUV constant for the max. value of V (vertical)

	// ========================================================================================================
	// 		 Precomputed YUV constants to avoid runtime divisions (increases performance and efficiency)
	// ========================================================================================================
	constexpr static float	 INV_255 = 1.0f / 255.0f;			// pre divided constant

	constexpr static float	 INV_k_R = 1.0f / (k_G + k_B);		// pre divided constant. (1 - k_R) = k_G + k_B
	constexpr static float	 INV_k_B = 1.0f / (k_R + k_G);		// pre divided constant. (1 - k_B) = k_R + k_G
	constexpr static float	 INV_k_G = 1.0f / k_G;				// pre divided constant

	constexpr static float INV_U_max = 1.0f / U_max;			// pre divided constant
	constexpr static float INV_V_max = 1.0f / V_max;			// pre divided constant


	/**
	 * @brief Represents a pixel in its YUV (+ Alpha channel) values according to the RGBA values
	 * @note The data structure used during image processing (true grayscale)
	 */
	struct alignas(16) Pixel
	{
		float Y;	// Y channel = Intensity [0, 1]
		float U;	// Difference between the B(lue) channel and the Y channel (horizontal) [-U_max, +U_max]
		float V;	// Difference between the R(ed) channel and the Y channel (vertical) [-V_max, +V_max]
		float A;	// Alpha channel (opacity) [0, 1]
	};

	/**
	 * @brief Represents an image and its essential data (quick access)
	 */
	struct Image
	{
		path path;						// path of this image source file
		std::vector<Pixel> pxls;		// image data as `Pixel` type
		int w;							// width of the image
		int h;							// height of the image
		bool stableAlpha;				// `true` if all pixels share the same alpha-channel value
	};


	std::array<double, 256> CDF;				// temporary cumulative histogram of a selected image (percentage)
	std::vector<Image> images;					// all images currently available in this session
	std::vector<uint8_t> tempRGBA;				// temporary RGBA data, copied from stb_data (loadImage) or converted by toYUV() (selectImage)

	time_point t_start;							// start time
	time_point t_end;							// end time
	duration dur;								// duration in ms

	int WIDTH;									// image width
	int HEIGHT;									// image height
	int CHANNELS;								// image channels (4 = RGBA is used)

	uint8_t IDX;								// current selected image index (images[IDX])
	uint8_t idxCDF;								// image index of the current active CDF

	void toYUV();								// RGBA -> YUV
	void toRGBA();								// YUV -> RGBA
	void computeCDF();							// CDF = Cumulative Distribution Function (W * H -> 256)
	uint8_t quantize(float f);					// [0.0, 1.0] -> [0, 255]
	void showDuration(const char* text);
};

#pragma once

#include "Types.hpp"

#include <filesystem>
#include <algorithm>
#include <vector>
#include <chrono>



namespace PixelStudio
{
	/**
	 * @brief Provides a professional and efficient image processing environment including feature detection.
	 * @note Uses externally the "stb" library by [Sean Barrett](https://github.com/nothings/stb) exclusively for reading and writing image files.
	 */
	class ImageProcessor
	{

	// ===================================================================================================================================================
	// =============================================================   P  R  I  V  A  T  E   =============================================================
	// ===================================================================================================================================================

	private:
		using clock = std::chrono::steady_clock;											// alias
		using time_point = clock::time_point;												// alias

		inline static uint32_t s_stamp = 0;													// uniqie stamp counter

		/**
		 * @brief Beyond this crossover point, RAM bandwidth dominates over L3 cache size.
		 * @note 16 MiP (2^24 = 16,777,216 Pixel / ~4096 x 4096)
		 */
		constexpr static size_t MP_16_THRESHOLD = 1ULL << 24;

		// -----------------------------------------------------------------------------------------------------------------------------------------------
		// 													Original YUV constants (YCbCr Model)
		// -----------------------------------------------------------------------------------------------------------------------------------------------
		constexpr static float k_R	 = 0.299f;							// YUV constant for the RGB R-channel
		constexpr static float k_G	 = 0.587f;							// YUV constant for the RGB G-channel. k_G = 1 - (k_R + k_B)
		constexpr static float k_B	 = 0.114f;							// YUV constant for the RGB B-channel

		constexpr static float k_U	 = 0.500f;							// YUV constant for the color difference (blue, horizontal)
		constexpr static float k_V	 = 0.500f;							// YUV constant for the color difference (red, vertical)

		constexpr static float U_max = 0.564f;							// YUV constant for the max. value of U (horizontal)
		constexpr static float V_max = 0.713f;							// YUV constant for the max. value of V (vertical)

		// -----------------------------------------------------------------------------------------------------------------------------------------------
		// 						Precomputed YUV constants to avoid runtime divisions (increases performance and efficiency)
		// -----------------------------------------------------------------------------------------------------------------------------------------------
		constexpr static float INV_255	 = 1.0f / 255.0f;				// pre divided constant

		constexpr static float INV_k_R	 = 1.0f / (k_G + k_B);			// pre divided constant. (1 - k_R) = k_G + k_B
		constexpr static float INV_k_B	 = 1.0f / (k_R + k_G);			// pre divided constant. (1 - k_B) = k_R + k_G
		constexpr static float INV_k_G	 = 1.0f / k_G;					// pre divided constant

		constexpr static float INV_U_max = 1.0f / U_max;				// pre divided constant
		constexpr static float INV_V_max = 1.0f / V_max;				// pre divided constant


		/**
		 * @brief Represents an image and its essential data in an SoA manner to radically speed up the YUVA dependent processes by AVX2/AVX-512.
		 * @note The different `chan` values are used during the image processing as indicators of the state the alpha channel values.
		 * At saving the image (and selection of the user) the correct (3 or 4) value is passed to the "stb" library.
		 */
		struct Image
		{
			std::vector<float> Y;							// Y channel (overall Intensity) [0.0, 1.0]
			std::vector<float> U;							// Difference between the B(lue) channel and the Y channel (horizontal) [-U_max, +U_max]
			std::vector<float> V;							// Difference between the R(ed) channel and the Y channel (vertical) [-V_max, +V_max]
			std::vector<float> A;							// Alpha channel (opacity) [0.0, 1.0]
			uint32_t stamp = 0;
			int w = 0;										// width of the image
			int h = 0;										// height of the image
			int chan = 0;									// channels of the image (internally -> 0: fully transparent, 3: fully opaque, 4: Alpha is valid)
		};

		/**
		 * @brief
		 */
		struct Harris_Data
		{
			std::vector<float> Ix;							// Y * SobelX convolution of the image (horizontal)
			std::vector<float> Iy;							// Y * SobelY convolution of the image (vertical)
			std::vector<float> Ixx;							// Ixx * Gauss
			std::vector<float> Iyy;							// Iyy * Gauss
			std::vector<float> Ixy;							// Ixy * Gauss
			std::vector<float> R;							// Response value
			std::vector<Keypoint> keypoints;
			uint32_t stamp = 0;
		};


		std::array<double, 256> m_CDF;						// temporary cumulative histogram of a selected image (percentage)
		Harris_Data m_harris;								// temporary "Harris-Setevens Detector" data
		std::vector<Image> m_images;						// all images currently available in this session (consistent indices /w ImGui)
		std::vector<uint8_t> m_RGBA;						// temp. RGBA data converted by toRGBA()
		//std::vector<uint8_t> RGB;							// temp. RGB data converted by toRGB() (convolution results)
		std::vector<float> m_padImg;						// padded image (according to the filter dimensions) to convolve /w  the filter
		Filter m_def_Filter;								// user defined filter
		ImageBufferView m_bufferView;						// a view to the buffered RGBA and essential data (for the GUI)

		time_point m_t_start;								// start time
		double m_ms = 0;									// duration in ms

		size_t m_W = 0;										// curr image width  (used a lot at padding)
		size_t m_H = 0;										// curr image height (used a lot at padding)

		int m_IDX = -1;										// current selected image index (images[IDX])
		uint32_t m_PADstamp	   = 0;							// curr padding belongs to which stamp
		uint16_t m_CHUNK_SIZE  = 0xFFFF;					// optimal chunk size to run OpenMP loops (specific for the `Pixel` data structure)
		uint8_t  m_MAX_THREADS = 0xFF;						// optimal thread num to run OpenMP loops (MAX_THREADS = physical core num)
		uint8_t  m_fRad		   = 0xFF;						// the "radial" offset from the center to the vertical and horizontal borders of the filter


		Result toYUVA(Result& res);									// RGBA -> YUVA
		Result toRGBA(Result& res);									// YUVA -> RGBA
		//Result toRGB(Result& res);								//    Y -> RGB
		void computeCDF(Result& res);								// CDF = Cumulative Distribution Function (W * H -> 256)
		void setPadImg(Result& res, const Filter& filter);
		Result computeSobelXY(Result& res);
		Result computeTensorM_1x1D(Result& res, float k_factor);
		Result computeTensorM_2x1D(Result& res, float k_factor);
		Result extractKeypoints(float threshold);

		std::vector<uint8_t> create_RGB();

		/**
		 * @brief Increments and returns the next unique stamp.
		 * @return the next uinique `uint32_t` stamp
		 */
		uint32_t nextStamp() { return ++s_stamp; }

		/**
		 * @brief Quantizes the given `float` value in the interval [0.0, 1.0] and returns it as a commercially rounded `uint8_t` value in the range [0 – 255].
		 * @param f the `float` number to be quantized in the interval [0.0, 1.0]
		 * @return the quantized `uint8_t` number in the interval [0, 255]
		 */
		[[nodiscard]] constexpr uint8_t quantize(float f) noexcept
		{
			f = std::clamp(f, 0.0f, 1.0f);        					// clamping the range [0.0, 1.0]
			f = f * 255.0f + 0.5f;                					// scaling up to [0, 255] and round commercially (step 1)
			return static_cast<uint8_t>(f);       					// this cast finalizes the commercialy rounding (step 2)
		}

		/**
		 * @brief Caltulates and returns the elapsed time in milliseconds since the member `time_point t_start` until this function call
		 * @param t_end the exact `time_point` passed by `clock::now()` to the function
		 * @return returns the elapsed time in milliseconds since the member `time_point t_start` until the function call
		 */
		inline double toMS(time_point t_end) noexcept { return duration(t_end - m_t_start).count(); }


	// ===================================================================================================================================================
	// ==============================================================   P  U  B  L  I  C   ===============================================================
	// ===================================================================================================================================================

	public:
		ImageProcessor() = default;
		~ImageProcessor() = default;

		void init(Result &res);

		Result loadImage(const fs::path& loadPath);
		Result selectImage(int idx, Result& res);
		Result deleteImage(int idx);
		Result saveImage(const fs::path& savePath);

		Result addIntensity(float value);
		Result scaleIntensity(float factor);
		Result setContrast(float k);
		Result toNegative();
		Result applyHistogramEqualization();
		Result applySegmentation(float t, Result& res, uint8_t segmType = 0);
		Result applyAutoSegmentation(uint8_t segmType = 0);
		Result posterize(uint8_t exp);
		Result setAlpha(float val);

		Result applyStandardFilter(Filter f);
		Result applyGenericFilter(Filter f);
		Result applyHarrisXY(float sigma, float k, float threshold);

		void defineLOG(float sigma);
		float getBHT(Result &res);
		Result compareRGBA(int oIdx);


		uint32_t getHarrisStamp() { return m_harris.stamp; }

		/**
		 * @brief Returns the current `RGBA` [0, 255] packed /w additional info and the stamp as a `ImageBufferView` (helper struct)
		 * @return the image to draw packed as a `ImageBufferView` {span<RGBA>, stamp, width, height, chanCode}
		 */
		[[nodiscard]] const ImageBufferView& getImageBufferView() const noexcept { return m_bufferView; }

		/**
		 * @brief Returns the current `Harris_Stevens_Data` its `Ix` (horizontal convolution) luminance values at an interval of [0.0, 1.0] for displaying
		 * @return `std::span<const float>`
		 */
		[[nodiscard]] std::span<const float> get_Ix() const noexcept { return m_harris.Ix; }

		/**
		 * @brief Returns the current `Harris_Stevens_Data` its `Iy` (vertical convolution) luminance values at an interval of [0.0, 1.0] for displaying
		 * @return `std::span<const float>`
		 */
		[[nodiscard]] std::span<const float> get_Iy() const noexcept { return m_harris.Iy; }

		[[nodiscard]] std::span<const float> get_Ixx() const noexcept { return m_harris.Ixx; }
		[[nodiscard]] std::span<const float> get_Iyy() const noexcept { return m_harris.Iyy; }
		[[nodiscard]] std::span<const float> get_Ixy() const noexcept { return m_harris.Ixy; }

		// Standard-Keypoints nach applyHarris (mit aktuellem/default Threshold)
		[[nodiscard]] std::span<const Keypoint> getKeypoints() const noexcept { return m_harris.keypoints; }
		// Dynamische Filterung bei Slider-Change
		[[nodiscard]] std::span<const Keypoint> getKeypoints(float threshold);

	};
}

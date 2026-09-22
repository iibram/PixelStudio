#include "Types.hpp"
#include "SysInfo.hpp" // IWYU pragma: keep
#include "ImageProcessor.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <span>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"



namespace PixelStudio
{
	// =======================================================================================================================================================================
	// =======================================================================   P  U  B  L  I  C   ==========================================================================
	// =======================================================================================================================================================================

	/**
	 * @brief Invokes `SysInfo` for getting the optimal working sizes (MAX_THREADS and CHUNK_SIZE) for any parallel OpenMP of this system in this session. Also reserves a
	 * massive capacity (16K UHD m_images) for the `RGBA` vector - essential for both displaying the image on screen and saving the current image.
	 */
	void ImageProcessor::init(Result &res)
	{
		res.logs.push_back({TextCode::IP_Init, {}});

		#ifdef PARALLEL_RUN
		// -----------------------------------------------------------------------
		// pre-setting the optimal OpenMP loop dimensions (for the running system)
		// -----------------------------------------------------------------------
		uint8_t m_MAX_THREADS = SysInfo::getPhysicalCoreNum();													// get optimal thread num to run OMP loops (phys. core num)
		omp_set_num_threads(m_MAX_THREADS);																		// set that result for any OpenMP loop in this session
		res.logs.push_back({TextCode::IP_MAX_THREADS, {static_cast<int>(m_MAX_THREADS)}});

		// target data structure: a `Pixel` element has (4 * float = 16 bytes)
		m_CHUNK_SIZE = SysInfo::getChunkSize(sizeof(float) << 2);												// get optimal chunk size for OMP loops
		res.logs.push_back({TextCode::IP_CHUNK_SIZE, {static_cast<int>(m_CHUNK_SIZE)}});
		#endif

		// reserve a massive capacity of 16K UHD resolution RGBA images for the `RGBA` and the `padImg` vectors (avoid reallocation during runtime)
		const size_t MAX_16K_BYTES = 15360ULL * 8640ULL * 4ULL;
		m_RGBA.reserve(MAX_16K_BYTES);
		m_padImg.reserve(MAX_16K_BYTES >> 2);

		res.logs.push_back({TextCode::IP_RGBA_resize, {}});
	}

	/**
	 * @brief Loads the image file from the specified path and reports the success status. When successfully, the RGBA of the image are stored at `RGBA`, are converted to
	 * the YUVA SoA structure, and the image is pushed to the `m_images` vector.
	 * @param ID the unique ID of the image
	 * @param loadPath the image file to load from
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::loadImage(const fs::path& loadPath)
	{
		int W = 0, H = 0, channels = 0;

		m_t_start = clock::now();
		uint8_t* stb_data = stbi_load(loadPath.string().c_str(), &W, &H, &channels, 4);							// req_comp = 4 -> all m_images with alpha channel
		m_ms = toMS(clock::now());

		if (!stb_data)
		{
			const char* rawReason = stbi_failure_reason();
			std::string errReason = rawReason ? rawReason : "Unknown error";

			return Result {.logs = {{TextCode::IP_stb_data_Failed, {loadPath.generic_string(), errReason}}}};
		}

		m_IDX = static_cast<int>(m_images.size());

		Result res {.logs = {{TextCode::IP_Load_IDX, {m_IDX}}}};
		res.logs.push_back({TextCode::Metrics, {"load image", m_ms}});

		// ----------------------------------------------------------------
		// 						m_RGBA <- stb_data
		// ----------------------------------------------------------------
		m_t_start = clock::now();
		size_t SIZE = static_cast<size_t>(W) * H * 4;															// SIZE = data of stb_data -> (r,g,b,a,r,g,b,a...)
		m_RGBA.resize(SIZE);

		std::memcpy(m_RGBA.data(), stb_data, SIZE);
		m_ms = toMS(clock::now());

		stbi_image_free(stb_data);																				// free(stb_data)

		res.logs.push_back({TextCode::Metrics, {"RGBA < stb", m_ms}});

		// ----------------------------------------------------------------
		// 					m_images[m_IDX] <- YUVA data
		// ----------------------------------------------------------------
		Image img {.stamp = nextStamp(), .w = W, .h = H, .chan = channels};
		SIZE >>= 2;																								// size of each Y,U,V,A (SIZE / 4 => pixel)

		img.Y.resize(SIZE);
		img.U.resize(SIZE);
		img.V.resize(SIZE);
		img.A.resize(SIZE);

		m_images.push_back(std::move(img));

		// setting the YUVA SoA
		toYUVA(res);																							// sets .success accordingly itself

		// updating the RGBA buffer view (internal chanCode is set by toYUVA())
		m_bufferView.stamp = m_images[m_IDX].stamp;
		m_bufferView.width = W;
		m_bufferView.height = H;
		m_bufferView.chanCode = m_images[m_IDX].chan;
		m_bufferView.RGBA = std::span<const uint8_t>(m_RGBA.data(), (SIZE << 2));

		m_W = static_cast<size_t> (W);																			// m_W and m_H are ONLY set by load | select ! (*)
		m_H = static_cast<size_t> (H);

		return res;
	}

	/**
	 * @brief Switches the context to the `Image` (YUVA) referenced by the provided index (actual selected image by the user through the GUI).
	 * @param idx the index to switch to the YUVA datastructure of the image
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::selectImage(int idx, Result& res)
	{
		m_IDX = idx;

		m_W = static_cast<size_t>(m_images[m_IDX].w);															// m_W and m_H are ONLY set by load | select ! (*)
		m_H = static_cast<size_t>(m_images[m_IDX].h);

		res.logs.push_back({TextCode::IP_Select_IDX, {idx}});
		res.success = true;

		return res;
	}

	/**
	 * @brief Erases the selected image data from the `m_images` vector referenced by the passed index and returns `Result`s.
	 * @note `PixelStudioApp` will immediately pass an index to focus on, if there are any images left in this session.
	 * @param idx the index of the element to erase from the `m_images` vector
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::closeImage(int idx)
	{
		m_images.erase(m_images.begin() + idx);

		m_IDX = static_cast<int>(m_images.size()) - 1;

		return Result {.success = true, .logs = {{TextCode::IP_Delete_IDX, {idx}}}};
	}

	/**
	 * @brief Exports and saves the currently selected image to the provided (or default) path and reports the success status. The encoding is done by the "stb" library,
	 * but depending on the user selection and by the channels of the image, the most detailed & efficient export (targeting PNG) is selected for the export.
	 * @param savePath the filepath to save the image to
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::saveImage(const fs::path& savePath)
	{
		Result res {.success = true};																			// preset success

		if (m_bufferView.stamp != m_images[m_IDX].stamp) toRGBA(res);											// user could just select another image and press save!

		if (!res.success) 																						// if so -> toRGBA() failed?
		{
			res.logs.push_back({TextCode::IP_Save_toRGBA_Failed, {}});
			return res;
		}

		m_t_start = clock::now();

		std::string extension = savePath.extension().string();
		const int exportChans = (m_images[m_IDX].chan == 3) ? 3 : 4;

		int width	= m_images[m_IDX].w;
		int height	= m_images[m_IDX].h;
		int success = 0;

		const uint8_t* exportData = m_RGBA.data();																// a kind of forward declaration
		int strideBytes = (width << 2);
		std::vector<uint8_t> rgbStorage;																		// a kind of forward declaration

		const bool isJPG = (extension == ".jpg" || extension == ".jpeg");										// JPG/JPEG always forces 3 channels
		const bool needsRGB = (exportChans == 3 || isJPG);

		if (needsRGB)
		{
			rgbStorage = create_RGB();																			// RGB = `RGBA` excluded by the Alpha channel
			exportData = rgbStorage.data();																		// point exportData to RGB
			strideBytes = m_images[m_IDX].w * 3;																// 3 Bytes per pixel instead of 4
		}

		if (isJPG)
			success = stbi_write_jpg(savePath.string().c_str(), width, height, 3, exportData, 90);

		else if (extension == ".bmp")
			success = stbi_write_bmp(savePath.string().c_str(), width, height, exportChans, exportData);

		else if (extension == ".tga")
			success = stbi_write_tga(savePath.string().c_str(), width, height, exportChans, exportData);

		else // default = png
			success = stbi_write_png(savePath.string().c_str(), width, height, exportChans, exportData, strideBytes);


		m_ms = toMS(clock::now());
		res.logs.push_back({TextCode::Metrics, {"save image", m_ms}});

		if (success)
			res.logs.push_back({TextCode::IP_Save_IDX, {m_IDX}});												// preset .success = true or the result of toRGBA() is used
		else
		{
			res.logs.push_back({TextCode::IP_stb_write_Failed, {savePath.generic_string()}});
			res.success = false;
		}

		return res;
	}


	// =======================================================================================================================================================================
	// 																G L O B A L   I M A G E   P R O C E S S I N G
	// =======================================================================================================================================================================

	/**
	 * @brief Manipulates the overall intensity (Y [0, 1]) of the selected image by adding the passed value [-1, 1].
	 * @note Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param value the floating point value [-1, 1] to manipute the intesity by addition
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::addIntensity(float value)
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		if (value == 0.0f) return Result {.logs = {{TextCode::IP_Skip_Intensity, {}}}};							// => no any change in intenisty

		m_t_start = clock::now();

		const uint32_t size = m_images[m_IDX].Y.size();
		float* __restrict Y = m_images[m_IDX].Y.data();

		if (value > 0.0f)																						// positive?
		{
			for (uint32_t i = 0; i < size; ++i)
				Y[i] = std::min(Y[i] + value, 1.0f);
		}
		else																									// negative?
		{
			for (uint32_t i = 0; i < size; ++i)
				Y[i] = std::max(Y[i] + value, 0.0f);
		}
		m_ms = toMS(clock::now());

		Result res {.logs = {{TextCode::Metrics, {"added Y(I)", m_ms}}}};
		m_images[m_IDX].stamp = nextStamp();																	// stamp this manipulation

		return toRGBA(res);																						// returns `Result` accordingly itself
	}

	/**
	 * @brief Manipulates the overall intensity (Y [0, 1]) of the selected image by percentually scaling the passed factor [-1, 1].
	 * @note Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param factor the floating point factor [-1, 1] to manipute the intesity by scaling
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::scaleIntensity(float factor)
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		if (factor == 0.0f) return Result {.logs = {{TextCode::IP_Skip_Intensity, {}}}};						// => no any change in intenisty (ImGui slider ranges are safe)

		m_t_start = clock::now();

		const uint32_t size = m_images[m_IDX].Y.size();
		float* __restrict Y = m_images[m_IDX].Y.data();
		const float mult = 1.0f + factor;

		for (uint32_t i = 0; i < size; ++i)
			Y[i] = std::min(Y[i] * mult, 1.0f);

		m_ms = toMS(clock::now());

		Result res {.logs = {{TextCode::Metrics, {"scale Y(I)", m_ms}}}};
		m_images[m_IDX].stamp = nextStamp();																	// stamp this manipulation

		return toRGBA(res);																						// returns `Result` accordingly itself
	}

	/**
	 * @brief Manipulates the overall contrast of the selected image.
	 * @note Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param k the factor to manipulate the contrast [-1, 3]
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::setContrast(float k)
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		if (k == 1.0f) return Result {.logs = {{TextCode::IP_Skip_Contrast_NoOp, {}}}};							// => no any change in contrast

		if (k == -1.0f) return toNegative();																	// => inverts the image - returns `Result` via toRGBA()

		const uint32_t size = m_images[m_IDX].Y.size();
		float* __restrict Y = m_images[m_IDX].Y.data();

		if (k == 0.0f)																							// => set each Y = 0.5f
		{
			m_t_start = clock::now();

			for (uint32_t i = 0; i < size; ++i)
				Y[i] = 0.5f;

			m_ms = toMS(clock::now());

			Result res {.logs = {{TextCode::Metrics, {"m contrast", m_ms}}}};
			m_images[m_IDX].stamp = nextStamp();																// stamp this manipulation

			return toRGBA(res);																					// returns `Result` accordingly itself
		}

		else																									// ImGui slider ranges are safe
		{
			m_t_start = clock::now();
			const float offset = (1.0f - k) * 0.5f;

			for (uint32_t i = 0; i < size; ++i)
				Y[i] = std::clamp(k * Y[i] + offset, 0.0f, 1.0f);

			m_ms = toMS(clock::now());

			Result res {.logs = {{TextCode::Metrics, {"m contrast", m_ms}}}};
			m_images[m_IDX].stamp = nextStamp();																// stamp this manipulation

			return toRGBA(res);																					// returns `Result` accordingly itself
		}
	}

	/**
	 * @brief Inverts each Y value so the image become the true negative.
	 * @note Also called by `setContrast(k)`, when k = -1.0f.
	 * @note Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::toNegative()
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		m_t_start = clock::now();

		const uint32_t size = m_images[m_IDX].Y.size();
		float* __restrict Y = m_images[m_IDX].Y.data();

		for (uint32_t i = 0; i < size; ++i)
			Y[i] = 1.0f - Y[i];

		m_ms = toMS(clock::now());

		Result res {.logs = {{TextCode::Metrics, {"m negative", m_ms}}}};
		m_images[m_IDX].stamp = nextStamp();																	// stamp this manipulation

		return toRGBA(res);																						// returns `Result` accordingly itself
	}

	/**
	 * @brief Performs automatic histogram equalization to increase the dynamic range of the selected image.
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::applyHistogramEqualization()
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		Result res;

		if (!computeCDF(res).success)																			// computCDF() has failed
		{
			res.logs.push_back({TextCode::IP_HistoEqual_CDF_Failed, {}});
			return res;
		}

		m_t_start = clock::now();

		const uint32_t size = m_images[m_IDX].Y.size();
		float* __restrict Y = m_images[m_IDX].Y.data();

		#ifdef PARALLEL_RUN
		#pragma omp parallel for proc_bind(close) schedule(guided, m_CHUNK_SIZE)
		#endif
		for (uint32_t i = 0; i < size; ++i)
			Y[i] = static_cast<float>(m_CDF[quantize(Y[i])]);

		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"HistoEqual", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"HistoEqual", m_ms}});
		#endif

		m_images[m_IDX].stamp = nextStamp();																	// stamp this manipulation

		return toRGBA(res);																						// returns `Result` accordingly itself
	}

	/**
	 * @brief Computes the segmentation of the selected image into two classes (background, foreground) separated an automatic calculated threshold (by the BHT method).
	 * By supplying `SegmType` options the user can modify the two classes appearance, or can be left empty and the standard segmentation will be applied.
	 * @note Actually this function just calls `getBHT()` and invokes then `applySegmentation(t)` by passing the returned (t)hreshhold by the BHT algorithm.
	 * @param segmType `SegmType` enum {FOREGROUND_KEEP_COLOR = 1, BACKGROUND_TRANSPARENT = 2}. default/standard = 0 (BG: white, FG: black)
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::applyAutoSegmentation(uint8_t segmType)
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		Result res;
		float t = getBHT(res);

		if (t == -1.0f)																							// getBHT() <- computeCDF() has failed?
		{
			res.logs.push_back({TextCode::IP_AutoSegm_CDF_Failed, {}});
			return res;
		}

		if (!res.success)																						// getBHT() iself has failed?
		{
			res.logs.push_back({TextCode::IP_AutoSegm_BHT_Failed, {}});
			return res;
		}

		return applySegmentation(t, res, segmType);																// returns `Result` accordingly via toRGBA()
	}

	/**
	 * @brief Computes the segmentation of the selected image into two classes (background, foreground) separated by a user-defined threshold. By supplying `SegmType` options
	 * the user can modify the two classes appearance, or can be left empty and the standard segmentation will be applied.
	 * @note Could be called by the user or by `applyAutoSegmentation(..)` -> thus Alpha == 0.0f check seems to be redundant.
	 * @note When called by user inputs: Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param t the threshold value in the range [0.0, 1.0] above which the intensity values ​​(Y) is accepted as background
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 * @param segmType `SegmType` enum {FOREGROUND_KEEP_COLOR = 1, BACKGROUND_TRANSPARENT = 2}. default/standard = 0 (BG: white, FG: black)
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::applySegmentation(float t, Result& res, uint8_t segmType)
	{
		res.success = false;

		if (m_images[m_IDX].chan == 0)																			// skip manip. of fully transparent images
		{
			res.logs.push_back({TextCode::IP_Skip_Alpha0, {}});
			return res;
		}

		m_t_start = clock::now();

		const bool keepColor	 = (segmType & SegmType::FOREGROUND_KEEP_COLOR);
		const bool bgTransparent = (segmType & SegmType::BACKGROUND_TRANSPARENT);

		bool allZero = bgTransparent;
		bool allOnes = !bgTransparent;

		const uint32_t size = m_images[m_IDX].Y.size();

		float* __restrict Y = m_images[m_IDX].Y.data();
		float* __restrict U = m_images[m_IDX].U.data();
		float* __restrict V = m_images[m_IDX].V.data();
		float* __restrict A = m_images[m_IDX].A.data();

		for (uint32_t i = 0; i < size; ++i)
		{
			if (Y[i] < t) // ------------------- FOREGROUND -------------------
			{
				if (!keepColor)
				{
					Y[i] = 0.0f;																				// full black foreground (modes 0 & 2)
					U[i] = 0.0f;
					V[i] = 0.0f;
					A[i] = 1.0f;																				// full opaque foreground
				}
				// keep color => don't touch YUVA (mode 1 & 3)
				allZero &= (A[i] == 0.0f);																		// but check if foreground Alpha is "allZero"
				allOnes &= (A[i] == 1.0f);																		// or "allOnes"
			}

			else // ---------------------------- BACKGROUND -------------------
			{
				if (bgTransparent) A[i] = 0.0f;																	// YUV stays untouched but A = 0 (modes 2 & 3)

				else																							// background full white & A = 1 (modes 0 & 1)
				{
					Y[i] = 1.0f;
					U[i] = 0.0f;
					V[i] = 0.0f;
					A[i] = 1.0f;
				}
			}
		}

		m_ms = toMS(clock::now());

		if (bgTransparent)
			m_images[m_IDX].chan = allZero ? 0 : 4;
		else
			m_images[m_IDX].chan = allOnes ? 3 : 4;

		res.logs.push_back({TextCode::Metrics, {"apply Segm", m_ms}});
		m_images[m_IDX].stamp = nextStamp();																	// stamp this manipulation

		return toRGBA(res);																						// returns `Result` accordingly itself
	}

	/**
	 * @brief Posterizes the selected image using the supplied 2^{exp} value. The range of the RGBA values are reduced according to the exponent.
	 * @note This function is performing on the RGBA data instead of YUVA, so it must check (*) if the RGBA data belongs to the image this function is called on.
	 * @note Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param exp the exponent [1, 7] influences the range of aggregated values
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::posterize(uint8_t exp)
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		Result res {.success = true};																			// oreset success
		if (m_bufferView.stamp != m_images[m_IDX].stamp) toRGBA(res);											// does `RGBA` belongs to this images YUVA? (*)

		if (!res.success)																						// if so -> toRGBA() failed?
		{
			res.logs.push_back({TextCode::IP_Posterize_toRGBA_Failed, {}});
			return res;
		}

		m_t_start = clock::now();

		const uint8_t mask	 = static_cast<uint8_t>(0xFF << exp);
		const uint8_t center = static_cast<uint8_t>(1 << (exp - 1));
		const uint32_t size	 = static_cast<uint32_t>(m_RGBA.size());

		uint8_t* __restrict pRGBA = m_RGBA.data();

		#ifdef PARALLEL_RUN
		#pragma omp parallel for proc_bind(close) schedule(guided, (m_CHUNK_SIZE))
		#endif
		for (uint32_t k = 0; k < size; k += 4)
		{
			// masking and + center only RGB
			pRGBA[k + 0] = (pRGBA[k + 0] & mask) + center;
			pRGBA[k + 1] = (pRGBA[k + 1] & mask) + center;
			pRGBA[k + 2] = (pRGBA[k + 2] & mask) + center;
		}
		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"posterized", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"posterized", m_ms}});
		#endif

		// after manipulating directly the m_RGBA => updating the buffer view
		m_bufferView.stamp = m_images[m_IDX].stamp = nextStamp();												// stamp this manipulation
		m_bufferView.width = m_images[m_IDX].w;
		m_bufferView.height = m_images[m_IDX].h;
		m_bufferView.chanCode = m_images[m_IDX].chan;
		m_bufferView.RGBA = std::span<const uint8_t>(m_RGBA.data(), size);

		// also updating the YUVA SoA
		return toYUVA(res);																						// returns `Result` accordingly itself
	}

	/**
	 * @brief Sets each Alpha channel value of the image to the passed value in the range [0.0, 1.0].
	 * @note Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param value `float` value in the range [0.0, 1.0] to set the overall Alpha channel of the image to
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::setAlpha(float val)
	{
		m_t_start = clock::now();

		float* __restrict A = m_images[m_IDX].A.data();
		const uint32_t size = m_images[m_IDX].Y.size();

		#ifdef PARALLEL_RUN
		#pragma omp parallel for proc_bind(close) schedule(guided, (m_CHUNK_SIZE))
		#endif
		for (uint32_t i = 0; i < size; ++i)
			A[i] = val;

		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		Result res {.logs = {{TextCode::Metrics_Parallel, {"Alpha chan", m_ms}}}};
		#else
		Result res {.logs = {{TextCode::Metrics, {"Alpha chan", m_ms}}}};
		#endif

		// updating chanCode according to new Alpha value
		if (val == 0.0f)	  m_images[m_IDX].chan = 0;
		else if (val == 1.0f) m_images[m_IDX].chan = 3;
		else				  m_images[m_IDX].chan = 2;

		m_images[m_IDX].stamp = nextStamp();																	// stamp this manipulation

		return toRGBA(res);																						// returns `Result` accordingly itself
	}


	// =======================================================================================================================================================================
	// 																	L O C A L   I M A G E   P R O C E S S I N G
	// =======================================================================================================================================================================

	/**
	 * --- CURRENTLY NOT IN USE !!! ---
	 * @brief Occupies the `padImg` cells with the Y values of each pixel of the selected image and its dimesion accoringly by respecting the padding to perform a convolution
	 * with a filter by the passed dimesions.
	 * @param f the `Filter` to use for the comvolution
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::applyStandardFilter(Filter f)
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		Result res {.success = true};																			// preset success

		if (m_PADstamp == m_images[m_IDX].stamp && m_fRad == f.rad)												// `padImg` belongs to curr image & filter radius is unchanged?
			res.logs.push_back({TextCode::IP_Skip_Padding, {}});
		else
			setPadImg(res, f);

		if (!res.success)																						// padding has failed?
		{
			res.logs.push_back({TextCode::IP_Convolution_Padding_Failed, {}});
			return res;
		}

		m_t_start = clock::now();

		// uint32_t padH = m_H + (f.rad << 1);
		size_t padW = m_W + (f.rad << 1);
		size_t k = 0;

		// m_padImg.resize(static_cast<uint64_t>(padW) * padH);

		// for (uint64_t y = 0; y < m_H; ++y)
		// {
		// 	uint64_t row = (y + f.rad) * padW;

		// 	for (uint32_t x = 0; x < m_W; ++x)
		// 		m_padImg[row + x + f.rad] = m_images[m_IDX].Y[k++];
		// }

		k = 0;
		for (size_t y = 0; y < m_H; ++y)
		{
			size_t row = (y + f.rad) * padW;

			for (size_t x = 0; x < m_W; ++x)
			{
				size_t cen = row + f.rad + x;

				float valX = 0.5f;
				float valY = 0.5f;

				size_t up = cen - f.rad * padW;
				size_t dw = cen + f.rad * padW;

				valX += (m_padImg[up - 1] * f.kernel[0] + m_padImg[cen - 1] * f.kernel[3] + m_padImg[dw - 1] * f.kernel[6]);
				valX += (m_padImg[up + 1] * f.kernel[2] + m_padImg[cen + 1] * f.kernel[5] + m_padImg[dw + 1] * f.kernel[8]);

				valY += (m_padImg[up - 1] * f.kernel[0] + m_padImg[up] * f.kernel[1] + m_padImg[up + 1] * f.kernel[2]);
				valY += (m_padImg[dw - 1] * f.kernel[6] + m_padImg[dw] * f.kernel[7] + m_padImg[dw + 1] * f.kernel[8]);

				m_harris.Ix[k] = std::clamp(valX, 0.0f, 1.0f);
				m_harris.Iy[k] = std::clamp(valY, 0.0f, 1.0f);

				++k;
			}
		}

		m_ms = toMS(clock::now());

		res.logs.push_back({TextCode::Metrics, {"filter_3x3", m_ms}});
		//m_PADstamp = m_images[m_IDX].stamp = nextStamp();

		return toRGBA(res);																						// returns `Result` accordingly itself
	}

	/**
	 * --- CURRETLY NOT IN USE !!! ---
	 * @brief Occupies the `padImg` cells with the Y values of each pixel of the selected image and its dimesion accoringly by respecting the padding to perform a convolution
	 * with a filter by the passed dimesions.
	 * @param f the `Filter` to use for the convolution
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::applyGenericFilter(Filter f)
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		Result res {.success = true};																			// preset success

		if (m_PADstamp == m_images[m_IDX].stamp && m_fRad == f.rad)												// `padImg` belongs to curr image & filter radius is unchanged?
			res.logs.push_back({TextCode::IP_Skip_Padding, {}});
		else
			setPadImg(res, f);

		if (!res.success)																						// padding has failed?
		{
			res.logs.push_back({TextCode::IP_Convolution_Padding_Failed, {}});
			return res;
		}

		m_t_start = clock::now();

		size_t pad_H = m_H + (f.rad << 1);
		size_t pad_W = m_W + (f.rad << 1);

		// =============================================== the convolution of the `padImg` and the specified filter ==================================================

		#ifdef PARALLEL_RUN
		#pragma omp parallel for default(none) shared(m_H, m_W, pad_W, m_padImg, m_images, f, m_IDX) schedule(static)
		#endif
		for (size_t y = 0; y < m_H; ++y)
		{
			size_t img_row = y * m_W;

			for (size_t x = 0; x < m_W; ++x)
			{
				float conv_val = 0.5f;																			// resulting convolution value (grey offset = 0.5f)
				size_t k_idx = 0;																				// kernel index

				for (int ky = -f.rad; ky <= f.rad; ++ky)														// dynamic filter loop (3x3, 5x5x, 7x7,..., nxn)
				{
					size_t p_y = (y + f.rad + ky) * pad_W;														// absolute y coordinate of padImg (1D vector)

					for (int kx = -f.rad; kx <= f.rad; ++kx)
					{
						size_t p_x = x + f.rad + kx;															// absolute x coordinate of padImg (1D vector)
						size_t pImg_i = p_y + p_x;																// calculating 1D index

						conv_val += m_padImg[pImg_i] * f.kernel[k_idx++];										// accumulating the (padImg * filter) values
					}
				}

				conv_val = std::clamp(conv_val, 0.0f, 1.0f);													// clamping the convolution value

				size_t img_idx = img_row + x;

				m_images[m_IDX].Y[img_idx] = conv_val;
				m_images[m_IDX].U[img_idx] = 0.0f;
				m_images[m_IDX].V[img_idx] = 0.0f;
				m_images[m_IDX].A[img_idx] = 1.0f;
			}
		}
		// -----------------------------------------------------------------------------------------------------------------------------------------------------------

		m_images[m_IDX].chan = 3;																				// now the image is  fully opaque
		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"gen filter", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"gen filter", m_ms}});
		#endif

		return toRGBA(res);																						// returns `Result` accordingly itself
	}

	/**
	 * @brief Manages all steps of the "Harris Corner Detection" process /w the industry standard filters (currently sigma is NOT used !!!)
	 * @note Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param sigma is manipulating the diameter and the softness of the gaussian blurr kernel. (currently NOT in use !!!)
	 * @param k an empirical determined sensitivity parameter [0.04 - 0.06]. Lower: can detect false corners, Larger: risks missing valid corners
	 * @param threshold the minimum threshold that must be exceeded to classify a pixel as a corner
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::applyHarrisXY(float sigma, float k, float threshold)
	{
		if (m_images[m_IDX].chan == 0) return Result {.logs = {{TextCode::IP_Skip_Alpha0, {}}}};				// skip manip. of fully transparent images

		Result res {.success = true};																			// preset success
		Filter f = SOBEL_X;

		if (m_PADstamp == m_images[m_IDX].stamp && m_fRad == f.rad)												// `padImg` belongs to curr image & filter radius is unchanged?
			res.logs.push_back({TextCode::IP_Skip_Padding, {}});												// skip padding			(log 1)
		else																									// or
			setPadImg(res, f);																					// gen padImg metrics 	(log 1)

		if (!res.success)																						// padding has failed?
		{
			res.logs.push_back({TextCode::IP_Convolution_Padding_Failed, {}});
			return res;
		}

		const size_t size = m_images[m_IDX].Y.size();

		m_harris.Ix.resize(size);
		m_harris.Iy.resize(size);

		m_harris.Ixx.resize(size);
		m_harris.Iyy.resize(size);
		m_harris.Ixy.resize(size);

		m_harris.R.resize(size);
		m_harris.keypoints.clear();

		if (!computeSobelXY(res).success)																		// comp. Sobel metrics	(log 2)
		{
			res.logs.push_back({TextCode::IP_Convolution_Sobel_Failed, {}});									// when sobelXY failed
			return res;
		}

		// Dispatcher depends on Megapixel size
		if (size < MP_16_THRESHOLD)
		{
			if (computeTensorM_2x1D(res, k).success)															// gauss5_2x1D metrics	(log 3)
				m_harris.keypoints.reserve(1 << 16);															// resize at success
			else
			{
				res.logs.push_back({TextCode::IP_Convolution_Gauss_Failed, {}});								// when gaussian failed
				return res;
			}
		}
		else																									// or
		{
			if (computeTensorM_1x1D(res, k).success)															// gauss5_1x1D metrics	(log 3)
				m_harris.keypoints.reserve(1 << 18);															// resize at success
			else
			{
				res.logs.push_back({TextCode::IP_Convolution_Gauss_Failed, {}});								// when gaussian failed
				return res;
			}
		}

		return extractKeypoints(res, threshold);																// extr. k_pts metrics	(log 4) - returns `Result` itself
	}

	/**
	 * --- CURRENTLY NOT IN USE !!! ---
	 * @brief Compares the currently selected image with the image referenced by the specified index. Counts the differing pixels (in RGBA) if applicable and outputs the
	 * information. Reports any issues, specifying the cause.
	 * @param oIdx the index of the other image to compare the pixels
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::compareRGBA(int oIdx)
	{
		Result res {.success = true};																			// preset success

		if (m_bufferView.stamp != m_images[m_IDX].stamp) toRGBA(res);											// does `RGBA` belongs to this images YUVA? (*)

		if (!res.success)																						// toRGBA() failed ?
		{
			res.logs.push_back({TextCode::IP_Image_Compare_toRGBA_Failed, {}});
			return res;
		}
		res.success = false;																					// reset for next indication

		if (oIdx < m_images.size())
		{
			const size_t size = m_images[oIdx].Y.size();

			const float* __restrict Y = m_images[oIdx].Y.data();
			const float* __restrict U = m_images[oIdx].U.data();
			const float* __restrict V = m_images[oIdx].V.data();
			const float* __restrict A = m_images[oIdx].A.data();

			const uint8_t* __restrict pRGBA = m_RGBA.data();													// pRGBA points to the RGBA of the curr selected m_images[m_IDX]!

			if (m_images[m_IDX].Y.size() == size)
			{
				m_t_start = clock::now();
				int diff = 0;

				for (size_t i = 0; i < size; ++i)
				{
					size_t k = (i << 2);

					float R = Y[i] + (V[i] * INV_V_max);
					float B = Y[i] + (U[i] * INV_U_max);
					float G = (Y[i] - k_R * R - k_B * B) * INV_k_G;

					// compare: pRGBA <> m_images[oIdx] its converted RGBA (on thge fly)
					if (pRGBA[k + 0] != quantize(R) ||
						pRGBA[k + 1] != quantize(G) ||
						pRGBA[k + 2] != quantize(B) ||
						pRGBA[k + 3] != quantize(A[i]))
					{
						++diff;
					}
				}

				m_ms = toMS(clock::now());

				res.success = true;																				// success !
				res.logs.push_back({TextCode::Metrics_Comp, {m_IDX, oIdx, m_ms}});
				res.logs.push_back({TextCode::IP_Pixel_Diff, {diff}});
			}
			else
				res.logs.push_back({TextCode::IP_Skip_PixelComp_Size, {}});
		}
		else
			res.logs.push_back({TextCode::IP_Skip_PixelComp_NotAvailable, {}});

		return res;
	}


	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 																		H E L P E R   F U N C T I O N S
	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------

	/**
	 * --- NOT IMPLEMENTED YET !!! ----
	 * @brief
	 * @param sigma
	 */
	void ImageProcessor::defineLOG(float sigma)
	{
	}

	/**
	 * @brief An implementation of the "Balanced Histogram Threshold" method. Calculates and returns the threshold value [0, 1] based on the cumulative histogram of the
	 * selected image.
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 * @return the calculated threshold [0, 1] based on the cumulative histogram of the selected image
	 */
	float ImageProcessor::getBHT(Result &res)
	{
		// no Alpha0 quick exit (is currently called just internally -> callers are handling it)

		if (!computeCDF(res).success) return -1.0f;																// when CDF fails

		res.success = false;																					// reset for next indication

		uint8_t min = 0;
		uint8_t max = 255;
		uint8_t cen = 0;

		while ((min < 255) && (m_CDF[min] == m_CDF[min + 1]))
			min = min + 1;

		while ((max > 0) && (m_CDF[max] == m_CDF[max - 1]))
			max = max - 1;

		// res.logs.push_back({TextCode::IP_BHT, {min, max, cen}});
		// std::cout << std::format("min = {:3}, max = {:3}, cen = {:3}\n", min, max, cen);

		while (min < max)
		{
			cen = ((min + max) >> 1);

			if ((m_CDF[cen] - m_CDF[min]) < (m_CDF[max] - m_CDF[cen]))
				max = max - 1;
			else
				min = min + 1;
		}

		// res.logs.push_back({TextCode::IP_BHT, {min, max, cen, CDF[cen]}});
		// std::cout << std::format("min = {:3}, max = {:3}, cen = {:3}, CDF[cen] = t = {}\n", min, max, cen, CDF[cen]);

		res.success = true;

		return static_cast<float>(m_CDF[cen]);
	}

	/**
	 * @brief Evaluates and resturns the keypoints according to the set threshold while the threshold slider is just released.
	 * @note Incoming values are backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param threshold the minimum threshold that must be exceeded to classify a pixel as a corner (the threshold value at releasing the threshold slider)
	 * @return a lightweight span of `Keypoint`s {x, y, value}
	 */
	std::span<const Keypoint> ImageProcessor::getKeypoints(Result& res, float threshold)
	{
		m_harris.keypoints.clear();
		extractKeypoints(res, threshold);

		return m_harris.keypoints;
	}


	// =======================================================================================================================================================================
	// ======================================================================   P  R  I  V  A  T  E   ========================================================================
	// =======================================================================================================================================================================

	/**
	 * @brief Converts the current `RGBA` data into the YUVA SoA data structure and the channel code is set.
	 * @note During conversion, the Alpha channel values ​​are checked on variance. Since the pixels in most images share the same Alpha value, it makes sense to precalculate
	 * the conversion once and apply this value to every pixel. The key aspect of this approach is the significantly more efficient reuse of `toRGBA()`, which is called after
	 * every image processing step to display the result immediately on the screen.
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::toYUVA(Result &res)
	{
		res.success = false;

		m_t_start = clock::now();

		const size_t size = m_images[m_IDX].Y.size();

		float* __restrict Y = m_images[m_IDX].Y.data();
		float* __restrict U = m_images[m_IDX].U.data();
		float* __restrict V = m_images[m_IDX].V.data();
		float* __restrict A = m_images[m_IDX].A.data();

		const uint8_t* __restrict pRGBA = m_RGBA.data();

		if (m_images[m_IDX].chan != 4)																			// Alpha channel is uniform [0.0, 1.0]
		{
			const float fA = static_cast<float>(pRGBA[3]) * INV_255;

			for (size_t i = 0; i < size; ++i)
			{
				size_t k = i * 4;

				float R = static_cast<float>(pRGBA[k + 0]) * INV_255;
				float G = static_cast<float>(pRGBA[k + 1]) * INV_255;
				float B = static_cast<float>(pRGBA[k + 2]) * INV_255;

				float y = k_R * R + k_G * G + k_B * B;

				Y[i] = y;
				U[i] = k_U * (B - y) * INV_k_B;
				V[i] = k_V * (R - y) * INV_k_R;
				A[i] = fA;
			}
		}

		else																									// Alpha cnannel has legit values (kind will be set in this loop)
		{
			const float fAlpha0 = static_cast<float>(pRGBA[3]) * INV_255;										// fAlpha0 <- 1st Alpha of RGBA converted to float [0.0, 1.0]
			bool uniform = true;																				// assumption: Alpha channel is uniform

			// only makes sense to parallelize using Open MPI (message passing) !!!
			for (size_t i = 0; i < size; ++i)
			{
				size_t k = i * 4;

				float R  = static_cast<float>(pRGBA[k + 0]) * INV_255;
				float G  = static_cast<float>(pRGBA[k + 1]) * INV_255;
				float B  = static_cast<float>(pRGBA[k + 2]) * INV_255;
				float fA = static_cast<float>(pRGBA[k + 3]) * INV_255;

				if (fA != fAlpha0) uniform = false;																// uniform or not check

				float y = k_R * R + k_G * G + k_B * B;

				Y[i] = y;
				U[i] = k_U * (B - y) * INV_k_B;
				V[i] = k_V * (R - y) * INV_k_R;
				A[i] = fA;
			}

			if (uniform)																						// if all Alpha are uniform (curr chan is still 4)
			{
				if (fAlpha0 == 0.0f)
					m_images[m_IDX].chan = 0;																	// internal code 0 -> all Alpha 0.0
				else if (fAlpha0 == 1.0f)
					m_images[m_IDX].chan = 3;																	// internal code 3 -> all Alpha 1.0
				else
					m_images[m_IDX].chan = 2;																	// internal code 2 -> all Alpha uniform at 0.0 < x < 1.0
			}
		}
		m_ms = toMS(clock::now());

		res.logs.push_back({TextCode::Metrics, {"RGBA > YUV", m_ms}});
		res.success = true;

		return res;
	}

	/**
	 * @brief Reconstructs the RGBA values by the YUVA data of the selected image and stores the converted values in the `m_RGBA` vector, which is used to display the image
	 * after any modification efficiently.
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 * @return `Result` {success, logs}
	 */
	Result ImageProcessor::toRGBA(Result &res)
	{
		res.success = false;

		m_t_start = clock::now();

		const size_t size = m_images[m_IDX].Y.size();
		m_RGBA.resize(size << 2);

		const float* __restrict Y = m_images[m_IDX].Y.data();
		const float* __restrict U = m_images[m_IDX].U.data();
		const float* __restrict V = m_images[m_IDX].V.data();
		const float* __restrict A = m_images[m_IDX].A.data();

		uint8_t* __restrict pRGBA = m_RGBA.data();

		if (m_images[m_IDX].chan != 4)																			// uniform Alpha ?
		{
			const uint8_t qAlpha = quantize(A[0]);

			#ifdef PARALLEL_RUN
			#pragma omp parallel for proc_bind(close) schedule(guided, m_CHUNK_SIZE)
			#endif
			for (size_t i = 0; i < size; ++i)
			{
				size_t k = (i << 2);

				float R = Y[i] + (V[i] * INV_V_max);
				float B = Y[i] + (U[i] * INV_U_max);
				float G = (Y[i] - k_R * R - k_B * B) * INV_k_G;

				pRGBA[k + 0] = quantize(R);
				pRGBA[k + 1] = quantize(G);
				pRGBA[k + 2] = quantize(B);
				pRGBA[k + 3] = qAlpha;
			}
		}

		else																									// chan == 4 (Alpha probably varies)
		{
			#ifdef PARALLEL_RUN
			#pragma omp parallel for proc_bind(close) schedule(guided, m_CHUNK_SIZE)
			#endif
			for (size_t i = 0; i < size; ++i)
			{
				size_t k = (i << 2);

				float R = Y[i] + (V[i] * INV_V_max);
				float B = Y[i] + (U[i] * INV_U_max);
				float G = (Y[i] - k_R * R - k_B * B) * INV_k_G;

				pRGBA[k + 0] = quantize(R);
				pRGBA[k + 1] = quantize(G);
				pRGBA[k + 2] = quantize(B);
				pRGBA[k + 3] = quantize(A[i]);
			}
		}

		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"RGBA < YUV", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"RGBA < YUV", m_ms}});
		#endif

		m_bufferView.stamp = m_images[m_IDX].stamp;
		m_bufferView.width = m_images[m_IDX].w;
		m_bufferView.height = m_images[m_IDX].h;
		m_bufferView.chanCode = m_images[m_IDX].chan;
		m_bufferView.RGBA = std::span<const uint8_t>(m_RGBA.data(), (size << 2));								// updating the RGBA buffer view

		res.success = true;
		return res;
	}

	/**
	 * @brief Computes the "Probability Density Function" (PDF) and then the "Cumulative Distribution Function" (CDF) of the selected image its overall luminance (Y) and the
	 * values are temporarily stored in the `CDF` array.
	 * @note Computing CDF always runs serial and also there is no timing for CDF -> runs always for 256 elements at 0.000.. ms !!!
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 */
	Result ImageProcessor::computeCDF(Result &res)
	{
		// No extra Alpha0 quick exit (is called internally)

		res.success = false;

		m_t_start = clock::now();
		const size_t size	= m_images[m_IDX].Y.size();
		float* __restrict Y = m_images[m_IDX].Y.data();

		double PDF[256] = {0.0};
		m_CDF = {};

		const double INV_SIZE = (1.0 / static_cast<double>(size));

		// ==================================================================== computing PDF ========================================================================
		#ifdef PARALLEL_RUN
		#pragma omp parallel for proc_bind(close) schedule(guided, m_CHUNK_SIZE) reduction(+:PDF[:256])
		#endif
		for (size_t k = 0; k < size; ++k)
			PDF[quantize(Y[k])] += 1.0;																			// histogramm with 1.0 standard bins (to TEST)

		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"computePDF", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"computePDF", m_ms}});
		#endif

		// ------------------------------------------------------------------- continue serial -----------------------------------------------------------------------
		uint8_t i = 0;																							// auto resetable counter

		PDF[i] *= INV_SIZE;
		double sum = PDF[i];

		while (++i != 0)																						// terminates when i == 0 (auto reset i for reuse)
		{
			PDF[i] *= INV_SIZE;
			sum += PDF[i];
		}																										// now the historgram is a PDF (sum up for check if 1.0)

		// ==================================================================== computing CDF ========================================================================
		m_CDF[i] = PDF[i];																						// reuse of i, which was automatically reset to 0

		while (++i != 0)
			m_CDF[i] = m_CDF[i - 1] + PDF[i];

		res.logs.push_back({TextCode::IP_PDFsum_CDF255, {sum, m_CDF[255]}});
		res.success = true;

		return res;
	}

	/**
	 * @brief Generates a to the passed filter respectively padded version of the selected image and stores it directly at the temporary `padImg`.
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 * @param f the `Filter` including its dimension to wich the padding must show respect
	 */
	void ImageProcessor::setPadImg(Result &res, const Filter& f)
	{
		// No Alpha0 quick exit (function is called internally after such check)

		res.success = false;

		m_t_start = clock::now();

		const size_t fRad = f.rad;
		const size_t pad_W = m_W + (fRad << 1);
		const size_t pad_H = m_H + (fRad << 1);

		m_padImg.resize(pad_W * pad_H);

		const float* __restrict Y = m_images[m_IDX].Y.data();
		float*		 __restrict P = m_padImg.data();

		// =========================================================== constructing `m_padImg` 1D vector =============================================================

		// --------------------------  CENTER PADDING  -------------------------
		// 			(per line: left padding, image data, right padding)

		for (size_t y = 0; y < m_H; ++y)
		{
			const size_t src_offset = y * m_W;
			const size_t dst_offset = (y + fRad) * pad_W;

			const float* src_row = Y + src_offset;
			float* dst_row = P + dst_offset;

			std::fill_n(dst_row, fRad, src_row[0]);
			std::memcpy(dst_row + fRad, src_row, m_W * sizeof(float));
			std::fill_n(dst_row + fRad + m_W, fRad, src_row[m_W - 1]);
		}

		// --------------------------  UPPER PADDING  --------------------------

		const float* first_valid_row = P + (fRad * pad_W);
		for (size_t y = 0; y < fRad; ++y)
		{
			float* dst_top_row = P + (y * pad_W);
			std::memcpy(dst_top_row, first_valid_row, pad_W * sizeof(float));
		}

		// --------------------------  LOWER PADDING  --------------------------

		const float* last_valid_row = P + ((fRad + m_H - 1) * pad_W);
		for (size_t r = 0; r < fRad; ++r)
		{
			float* dst_bottom_row = P + ((fRad + m_H + r) * pad_W);
			std::memcpy(dst_bottom_row, last_valid_row, pad_W * sizeof(float));
		}

		m_PADstamp = m_images[m_IDX].stamp;																		// assign PADstamp /w the image stamp to which it belongs
		m_fRad = f.rad;																							// also store the radius of the used filter

		m_ms = toMS(clock::now());

		res.logs.push_back({TextCode::Metrics, {"gen padImg", m_ms}});
		res.success = true;
	}

	/**
	 * @brief Computes the convolution of the Y channel with the Sobel Filter in X and Y direction simultanously.
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 */
	Result ImageProcessor::computeSobelXY(Result& res)
	{
		res.success = false;

		m_t_start = clock::now();

		size_t fRad = 1;
		size_t pad_W = m_W + (fRad << 1);

		// =========================================== the convolution of the `m_padImg` /w Sobel in X & Y simultanously =============================================
		#ifdef PARALLEL_RUN
		#pragma omp parallel for default(none) shared(fRad, pad_W, m_padImg) proc_bind(close) schedule(guided, m_CHUNK_SIZE)
		#endif
		for (size_t y = 0; y < m_H; ++y)
		{
			size_t start = (y + fRad) * pad_W + fRad;

			for (size_t x = 0; x < m_W; ++x)
			{
				size_t cn = start + x;
				size_t up = cn - pad_W;
				size_t dw = cn + pad_W;

				// load Y values directly (L1 cache hit)
				float p00 = m_padImg[up - 1];	float p01 = m_padImg[up];	float p02 = m_padImg[up + 1];
				float p10 = m_padImg[cn - 1];								float p12 = m_padImg[cn + 1];
				float p20 = m_padImg[dw - 1];	float p21 = m_padImg[dw];	float p22 = m_padImg[dw + 1];

				// convolution /w SobelX
				float convX =
					(-1.0f * p00) + (1.0f * p02) +
					(-2.0f * p10) + (2.0f * p12) +
					(-1.0f * p20) + (1.0f * p22);

				// convolution /w SobelY
				float convY =
					(-1.0f * p00) + (-2.0f * p01) + (-1.0f * p02) +
					(+1.0f * p20) + (+2.0f * p21) + (+1.0f * p22);

				size_t k = y * m_W + x;

				// the RAW derivatives
				m_harris.Ix[k] = convX;
				m_harris.Iy[k] = convY;

				// quadratic terms for the structure tensor M
				m_harris.Ixx[k] = convX * convX;
				m_harris.Iyy[k] = convY * convY;
				m_harris.Ixy[k] = convX * convY;

				// when Orientation/Angles are in interest (maybe in future updates)
				// -----------------------------------------------------------------
				// m_harris.Magnitude[k] = std::sqrt(convX * convX + convY * convY);
				// m_harris.Angle[k]	  = std::atan2(convY, convX);
				// -----------------------------------------------------------------

				// RAW data served their purpose -> in-place conversion for display!
				m_harris.Ix[k] = std::clamp(convX + 0.5f, 0.0f, 1.0f);
				m_harris.Iy[k] = std::clamp(convY + 0.5f, 0.0f, 1.0f);
			}
		}
		// ===========================================================================================================================================================

		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"conv Sobel", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"conv Sobel", m_ms}});
		#endif

		m_harris.stamp = m_PADstamp;

		res.success = true;
		return res;
	}

	/**
	 * @brief Computes the convolution of Ixx, Iyy and Ixy with Gauss 5x5 1D filter simultanously and the Harris Response R values of the results.
	 * @note Incoming value is backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 * @param k_factor an empirical determined sensitivity parameter [0.04 - 0.06]. Lower: can detect false corners, Larger: risks missing valid corners
	 * @return
	 */
	Result ImageProcessor::computeTensorM_1x1D(Result & res, float k_factor)
	{
		res.success = false;

		m_t_start = clock::now();

		const size_t size = m_harris.Ixx.size();

		// temporary Gauusian blurr result buffers
		std::vector<float> tmpIxx(size);
		std::vector<float> tmpIyy(size);
		std::vector<float> tmpIxy(size);

		int H = static_cast<int>(m_H);
		int W = static_cast<int>(m_W);

		constexpr float W0 =  1.0f / 273.0f;	// 4 corners						[0] [1] [2] [1] [0]
		constexpr float W1 =  4.0f / 273.0f;	// 8 Outer Ring Neighbors			[1] [3] [4] [3] [1]
		constexpr float W2 =  7.0f / 273.0f;	// 4 Outer axle ends				[2] [4] [5] [4] [2]
		constexpr float W3 = 16.0f / 273.0f;	// 4 Inner corners					[1] [3] [4] [3] [1]
		constexpr float W4 = 26.0f / 273.0f;	// 4 Inner axis neighbors			[0] [1] [2] [1] [0]
		constexpr float W5 = 41.0f / 273.0f;	// 1 Center

		// ================================ the convolution of Tensor M =================================
		#ifdef PARALLEL_RUN
		#pragma omp parallel for default(none) shared(W, m_W, H, k_factor, tmpIxx, tmpIyy, tmpIxy)\
		proc_bind(close) schedule(guided, m_CHUNK_SIZE)
		#endif
		for (int y = 0; y < H; ++y)
		{
			// Y-Offset-Spannweite mit Border-Clamping
			int y0 = std::clamp(y - 2, 0, H - 1);
			int y1 = std::clamp(y - 1, 0, H - 1);
			int y2 = y;
			int y3 = std::clamp(y + 1, 0, H - 1);
			int y4 = std::clamp(y + 2, 0, H - 1);

			size_t r0 = m_W * static_cast<size_t>(y0);
			size_t r1 = m_W * static_cast<size_t>(y1);
			size_t r2 = m_W * static_cast<size_t>(y2);
			size_t r3 = m_W * static_cast<size_t>(y3);
			size_t r4 = m_W * static_cast<size_t>(y4);

			for (int x = 0; x < W; ++x)
			{
				size_t curr_i = r2 + static_cast<size_t>(x);

				int x0 = std::clamp(x - 2, 0, W - 1);
				int x1 = std::clamp(x - 1, 0, W - 1);
				int x2 = x;
				int x3 = std::clamp(x + 1, 0, W - 1);
				int x4 = std::clamp(x + 2, 0, W - 1);

				// ======================================================================================
				// 				Lambda helper for fast per-component symmetric accumulation
				// ======================================================================================
				auto calcSymmetricVal = [&](const std::vector<float>& src) -> float
				{
					// vcFormat-format off
					// clang-format off
					float g0 = (src[r0 + x0] + src[r0 + x4] + src[r4 + x0] + src[r4 + x4]);						// Group 0: 4 corners (weight W0)
					float g1 = (src[r0 + x1] + src[r0 + x3] + src[r4 + x1] + src[r4 + x3]
							  + src[r1 + x0] + src[r1 + x4] + src[r3 + x0] + src[r3 + x4]);						// Group 1: 8 outer ring neighbors (weight W1)
					float g2 = (src[r0 + x2] + src[r4 + x2] + src[r2 + x0] + src[r2 + x4]);						// Group 2: 4 outer axle ends (weight W2)
					float g3 = (src[r1 + x1] + src[r1 + x3] + src[r3 + x1] + src[r3 + x3]);						// Group 3: 4 inner corners (weight W3)
					float g4 = (src[r1 + x2] + src[r3 + x2] + src[r2 + x1] + src[r2 + x3]);						// Group 4: 4 inner axis neighbors (weight W4)
					float g5 = (src[r2 + x2]);																	// Group 5: 1 center (weight W5)

					return (g0 * W0) + (g1 * W1) + (g2 * W2) + (g3 * W3) + (g4 * W4) + (g5 * W5);				// after ALL additions -> only 6 multiplications!
					// clang-format on
					// vcFormat-format on
				};

				float sumIxx = calcSymmetricVal(m_harris.Ixx);
				float sumIyy = calcSymmetricVal(m_harris.Iyy);
				float sumIxy = calcSymmetricVal(m_harris.Ixy);

				// calculate Harris Response R from the now perfectly scaled values
				float det	= (sumIxx * sumIyy) - (sumIxy * sumIxy);								//   det(M) = Ixx * Iyy - (Ixy)^2 ==> (Ixx * Ixy)  - (Iyy * Ixy)
				float trace = sumIxx + sumIyy;														// trace(M) = Ixx + Iyy

				m_harris.R[curr_i] = det - k_factor * (trace * trace);								// R = det(M) - k * (trace(M))^2 ==> det(M) - k * (trace * trace)

				// write back final smoothed tensor values
				tmpIxx[curr_i] = sumIxx;
				tmpIyy[curr_i] = sumIyy;
				tmpIxy[curr_i] = sumIxy;
			}
		}

		// move the temporary result to the original buffers
		m_harris.Ixx = std::move(tmpIxx);
		m_harris.Iyy = std::move(tmpIyy);
		m_harris.Ixy = std::move(tmpIxy);

		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"Gauss 5 1D", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"Gauss 5 1D", m_ms}});
		#endif

		res.success = true;
		return res;
	}

	/**
	 * @brief Computes the convolution of Ixx, Iyy and Ixy with the Gauss 5x5 1D filter in X and Y direction of the filter, in separate loops. The Harris Response R values
	 * are also calculated of the results at the end of each loop.
	 * @note Incoming value is backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param res a container within the information pipeline to which messages can be appended at the end
	 * @param k_factor an empirical determined sensitivity parameter [0.04 - 0.06]. Lower: can detect false corners, Larger: risks missing valid corners
	 * @return
	 */
	Result ImageProcessor::computeTensorM_2x1D(Result& res, float k_factor)
	{
		res.success = false;

		m_t_start = clock::now();

		const size_t size = m_harris.Ixx.size();

		// temporary Gauusian blurr result buffers
		std::vector<float> tmpIxx(size);
		std::vector<float> tmpIyy(size);
		std::vector<float> tmpIxy(size);

		int H = static_cast<int>(m_H);
		int W = static_cast<int>(m_W);

		// =================================================================================================
		// 						   PASS 1: Horizontal convolution (X-direction, 1x5)
		// =================================================================================================
		#ifdef PARALLEL_RUN
		#pragma omp parallel for default(none) shared(W, m_W, H, GAUSS_1D, k_factor, tmpIxx, tmpIyy, tmpIxy)\
		proc_bind(close) schedule(guided, m_CHUNK_SIZE)
		#endif
		for (int y = 0; y < H; ++y)
		{
			for (int x = 0; x < W; ++x)
			{
				size_t curr_i = m_W * y + x;

				float sumIxx = 0.0f;
				float sumIyy = 0.0f;
				float sumIxy = 0.0f;

				for (int kx = -2; kx <= 2; ++kx)
				{
					int sampleX = std::clamp(x + kx, 0, W - 1);
					size_t sampleIdx = m_W * y + sampleX;

					float weight = GAUSS_1D.kernel[kx + 2];

					sumIxx += m_harris.Ixx[sampleIdx] * weight;
					sumIyy += m_harris.Iyy[sampleIdx] * weight;
					sumIxy += m_harris.Ixy[sampleIdx] * weight;
				}

				tmpIxx[curr_i] = sumIxx;
				tmpIyy[curr_i] = sumIyy;
				tmpIxy[curr_i] = sumIxy;
			}
		}

		// =================================================================================================
		// 				PASS 2: Vertical convolution (Y-direction, 5x1) + Calculate response R
		// =================================================================================================
		#ifdef PARALLEL_RUN
		#pragma omp parallel for default(none) shared(W, m_W, H, GAUSS_1D, k_factor, tmpIxx, tmpIyy, tmpIxy)\
		proc_bind(close) schedule(guided, m_CHUNK_SIZE)
		#endif
		for (int y = 0; y < H; ++y)
		{
			for (int x = 0; x < W; ++x)
			{
				size_t curr_i = m_W * y + x;

				float sumIxx = 0.0f;
				float sumIyy = 0.0f;
				float sumIxy = 0.0f;

				for (int ky = -2; ky <= 2; ++ky)
				{
					int sampleY = std::clamp(y + ky, 0, H - 1);
					size_t sampleIdx = m_W * sampleY + x;

					float weight = GAUSS_1D.kernel[ky + 2];

					sumIxx += tmpIxx[sampleIdx] * weight;
					sumIyy += tmpIyy[sampleIdx] * weight;
					sumIxy += tmpIxy[sampleIdx] * weight;
				}

				// calculate Harris Response R from the finally smoothed values
				float det   = (sumIxx * sumIyy) - (sumIxy * sumIxy);								//   det(M) = Ixx * Iyy - (Ixy)^2 ==> (Ixx * Ixy)  - (Iyy * Ixy)
				float trace = sumIxx + sumIyy;														// trace(M) = Ixx + Iyy

				m_harris.R[curr_i] = det - k_factor * (trace * trace);								// R = det(M) - k * (trace(M))^2 ==> det(M) - k * (trace * trace)

				// write back final smoothed tensor values
				m_harris.Ixx[curr_i] = sumIxx;
				m_harris.Iyy[curr_i] = sumIyy;
				m_harris.Ixy[curr_i] = sumIxy;
			}
		}

		// move the temporary result to the original buffers
		m_harris.Ixx = std::move(tmpIxx);
		m_harris.Iyy = std::move(tmpIyy);
		m_harris.Ixy = std::move(tmpIxy);

		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"Gauss 2x1D", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"Gauss 2x1D", m_ms}});
		#endif

		res.success = true;
		return res;
	}

	// /**
	//  * @brief Single loop
	//  * @param res
	//  * @param k_factor
	//  * @return
	//  */
	// Result ImageProcessor::computeTensorM(Result & res, float k_factor)
	// {
	// 	m_t_start = clock::now();

	// 	const size_t size = m_harris.Ixx.size();

	// 	std::vector<float> tmpIxx(size);
	// 	std::vector<float> tmpIyy(size);
	// 	std::vector<float> tmpIxy(size);

	// 	int H = static_cast<int>(m_H);
	// 	int W = static_cast<int>(m_W);

	// 	// ================================ the convolution of Tensor M =================================
	// 	#ifdef PARALLEL_RUN
	// 	#pragma omp parallel for default(none) shared(W, m_W, H, k_factor, GAUSS_5, tmpIxx, tmpIyy, tmpIxy)\
	// 	proc_bind(close) schedule(guided, m_CHUNK_SIZE)
	// 	#endif
	// 	for (int y = 0; y < H; ++y)
	// 	{
	// 		for (int x = 0; x < W; ++x)
	// 		{
	// 			// SAUBERE INDEX-BERECHNUNG (Kein Akkumulieren!)
	// 			size_t curr_i = m_W * y + x;

	// 			float sumIxx = 0.0f;
	// 			float sumIyy = 0.0f;
	// 			float sumIxy = 0.0f;

	// 			// 5x5 Gauß-Kernel-Schleife
	// 			for (int ky = -2; ky <= 2; ++ky)
	// 			{
	// 				int sampleY = std::clamp(y + ky, 0, H - 1);

	// 				for (int kx = -2; kx <= 2; ++kx)
	// 				{
	// 					int sampleX = std::clamp(x + kx, 0, W - 1);

	// 					size_t sampleIdx = m_W * sampleY + sampleX;

	// 					// GAUSS_5 flach indizieren: (ky + 2) * 5 + (kx + 2)
	// 					float weight = GAUSS_5.kernel[(ky + 2) * 5 + (kx + 2)];

	// 					sumIxx += m_harris.Ixx[sampleIdx] * weight;
	// 					sumIyy += m_harris.Iyy[sampleIdx] * weight;
	// 					sumIxy += m_harris.Ixy[sampleIdx] * weight;
	// 				}
	// 			}

	// 			// Harris Response R berechnen
	// 			float det   = (sumIxx * sumIyy) - (sumIxy * sumIxy);
	// 			float trace = sumIxx + sumIyy;

	// 			m_harris.R[curr_i] = det - k_factor * (trace * trace);

	// 			tmpIxx[curr_i] = sumIxx;
	// 			tmpIyy[curr_i] = sumIyy;
	// 			tmpIxy[curr_i] = sumIxy;
	// 		}
	// 	}

	// 	m_harris.Ixx = std::move(tmpIxx);
	// 	m_harris.Iyy = std::move(tmpIyy);
	// 	m_harris.Ixy = std::move(tmpIxy);

	// 	m_ms = toMS(clock::now());

	// 	#ifdef PARALLEL_RUN
	// 	res.logs.push_back({TextCode::Metrics_Parallel, {"gaussian 5", m_ms}});
	// 	#else
	// 	res.logs.push_back({TextCode::Metrics, {"gaussian 5", m_ms}});
	// 	#endif

	// 	return toRGBA(res);
	// }

	/**
	 * @brief Extracts the `Keypoint`s of the Harris "R" response values.
	 * @note Incoming value is backed up by ImGui slider ranges and mustn't checked. ImGuiSliderFlags_NoInput are used !!!
	 * @param threshold the minimum threshold that must be exceeded to classify a pixel as a corner
	 * @return Result
	 */
	Result ImageProcessor::extractKeypoints(Result& res, float threshold)
	{
		res.success = false;

		const int W = static_cast<int>(m_W);
		const int H = static_cast<int>(m_H);

		m_t_start = clock::now();

		// for thread safety, each thread collects its own keypoints
		#ifdef PARALLEL_RUN
		#pragma omp parallel default(none) shared(W, m_W, H, threshold, m_harris) proc_bind(close)
		{
			std::vector<Keypoint> local_keypoints;																// each thread gets its own local keypoints vector
			local_keypoints.reserve(1 << 14);

			#pragma omp for schedule(guided, m_CHUNK_SIZE)
			for (int y = 1; y < H - 1; ++y)
			{
				size_t r_cen = m_W * y;
				size_t r_up1 = m_W * (y - 1);
				size_t r_dn1 = m_W * (y + 1);

				for (int x = 1; x < W - 1; ++x)
				{
					size_t i = r_cen + x;
					float val = m_harris.R[i];

					if (val <= threshold)																		// early exit if val is already <= the threshold
						continue;

					// NMS 3x3 check: 'val' strictly greater than al 8 neighbours?
					if (val <= m_harris.R[r_up1 + (x - 1)] ||
						val <= m_harris.R[r_up1 + x] ||
						val <= m_harris.R[r_up1 + (x + 1)] ||
						val <= m_harris.R[r_cen + (x - 1)] ||
						val <= m_harris.R[r_cen + (x + 1)] ||
						val <= m_harris.R[r_dn1 + (x - 1)] ||
						val <= m_harris.R[r_dn1 + x] ||
						val <= m_harris.R[r_dn1 + (x + 1)])
					{
						continue;																				// not a local maximum!
					}

					local_keypoints.push_back(Keypoint {x, y, val});											// thread-safe push-back into the local vector !
				}
			}

			// merging all local results into the main vector (single lock at the end)
			#pragma omp critical
			{
				m_harris.keypoints.insert(m_harris.keypoints.end(), local_keypoints.begin(), local_keypoints.end());
			}
		}

		#else // ----  SERIAL_RUN  ----
		for (int y = 1; y < H - 1; ++y)
		{
			size_t r_cen = m_W * y;
			size_t r_up1 = m_W * (y - 1);
			size_t r_dn1 = m_W * (y + 1);

			for (int x = 1; x < W - 1; ++x)
			{
				size_t i = r_cen + x;
				float val = m_harris.R[i];

				if (val <= threshold)																			// early exit if val is already <= the threshold
					continue;

				// NMS 3x3 check: 'val' strictly greater than al 8 neighbours?
				if (val <= m_harris.R[r_up1 + (x - 1)] ||
					val <= m_harris.R[r_up1 + x] ||
					val <= m_harris.R[r_up1 + (x + 1)] ||
					val <= m_harris.R[r_cen + (x - 1)] ||
					val <= m_harris.R[r_cen + (x + 1)] ||
					val <= m_harris.R[r_dn1 + (x - 1)] ||
					val <= m_harris.R[r_dn1 + x] ||
					val <= m_harris.R[r_dn1 + (x + 1)])
				{
					continue;																					// not a local maximum!
				}

				m_harris.keypoints.push_back(Keypoint {x, y, val});												// when arrived here, a corner is detected!
			}
		}
		#endif

		m_ms = toMS(clock::now());

		#ifdef PARALLEL_RUN
		res.logs.push_back({TextCode::Metrics_Parallel, {"ext. k_pts", m_ms}});
		#else
		res.logs.push_back({TextCode::Metrics, {"ext. k_pts", m_ms}});
		#endif

		res.success = true;
		return res;
	}

	/**
	 * @brief Returns a vector with only the RGB values taken from the `RGBA` member.
	 * @return the resulting RGB values /wo the Alpha channel
	 */
	std::vector<uint8_t> ImageProcessor::create_RGB()
	{
		// No Alpha0 quick exit (called internally)

		const size_t TOTAL_PXLS = (static_cast<size_t>(m_W) * m_H);
		std::vector<uint8_t> rgb(TOTAL_PXLS * 3);

		for (size_t i = 0; i < TOTAL_PXLS; ++i)
		{
			size_t j = i * 3;
			size_t k = i << 2;

			rgb[j + 0] = m_RGBA[k + 0];	// R
			rgb[j + 1] = m_RGBA[k + 1];	// G
			rgb[j + 2] = m_RGBA[k + 2];	// B
		}
		return rgb;
	}
}

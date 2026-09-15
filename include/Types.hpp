#pragma once

#include <string_view>
#include <filesystem>
#include <cstdint>
#include <variant>
#include <string>
#include <vector>
#include <format>
#include <chrono>
#include <array>
#include <span>



/**
 * @brief Declares some essential shared types of the whole application in the `PixelStudio` namespace
 */
namespace PixelStudio
{
	namespace fs = std::filesystem;														// alias

	static fs::path DEFAULT_LOAD_PATH = fs::current_path() / "default_DIR" / "IN";		// the default path to load images
	static fs::path DEFAULT_SAVE_PATH = fs::current_path() / "default_DIR" / "OUT";		// the defualt path to save images

	using duration = std::chrono::duration<double, std::milli>;							// alias
	using LogArg = std::variant<int, double, std::string>;								// alias

	// // ============================================== PixelStudioApp originated shared types ===============================================

	// enum GPU_Vendor : uint8_t { UNKNOWN, NVIDIA, AMD, Intel };

	// /**
	//  * @brief GPU Information Struct & Detection
	// */
	// struct GPUInfo
	// {
	// 	std::string vendor	 = "Unknown";
	// 	std::string renderer = "Unknown";
	// 	float inUse_VRAM	 = 0.0f;
	// 	float avail_VRAM	 = 0.0f;
	// 	GPU_Vendor gpuVendor = GPU_Vendor::UNKNOWN;
	// };

	// ============================================== ImageProcessor originated shared types ===============================================

	/**
	 * @brief Options during segmentations to modify the appearance of the back- and foreground
	 */
	enum SegmType : uint8_t { FOREGROUND_KEEP_COLOR = 1, BACKGROUND_TRANSPARENT = 2 };


	// -------------------------------------------------------------------------------------------------------------------------------------
	// 																Filter Kernels
	// -------------------------------------------------------------------------------------------------------------------------------------
	constexpr static float sobelX[9] =
	{
		-1.0f, 0.0f, 1.0f,
		-2.0f, 0.0f, 2.0f,
		-1.0f, 0.0f, 1.0f
	};
	constexpr static float sobelY[9] =
	{
		-1.0f, -2.0f, -1.0f,
		 0.0f,  0.0f,  0.0f,
		 1.0f,  2.0f,  1.0f
	};

	constexpr static float gauss5[25] =
	{
		1.0f / 273.0f,  4.0f / 273.0f,  7.0f / 273.0f,  4.0f / 273.0f, 1.0f / 273.0f,
		4.0f / 273.0f, 16.0f / 273.0f, 26.0f / 273.0f, 16.0f / 273.0f, 4.0f / 273.0f,
		7.0f / 273.0f, 26.0f / 273.0f, 41.0f / 273.0f, 26.0f / 273.0f, 7.0f / 273.0f,
		4.0f / 273.0f, 16.0f / 273.0f, 26.0f / 273.0f, 16.0f / 273.0f, 4.0f / 273.0f,
		1.0f / 273.0f,  4.0f / 273.0f,  7.0f / 273.0f,  4.0f / 273.0f, 1.0f / 273.0f
	};

	constexpr static float gauss5_1D[5] =
	{
		1.0f / 17.0f,  4.0f / 17.0f,  7.0f / 17.0f,  4.0f / 17.0f, 1.0f / 17.0f
	};

	constexpr static float negLog[25] =
	{
		 0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
		 0.0f, -1.0f, -2.0f, -1.0f,  0.0f,
		-1.0f, -2.0f, 16.0f, -2.0f, -1.0f,
		 0.0f, -1.0f, -2.0f, -1.0f,  0.0f,
		 0.0f,  0.0f, -1.0f,  0.0f,  0.0f
	};

	/**
	 * @brief Acts as a container for filters in different size and type (static/dynamic).
	 */
	struct Filter
	{
		std::span<const float> kernel;					// the filter kernel (provides the static/dynamic data)
		uint8_t rad;                 					// the radial of the center to the width and height of the filter
	};

	// predefined standard filters
	constexpr static Filter SOBEL_X	 = {sobelX, 1};		// Sobel filter X (horizontal)
	constexpr static Filter SOBEL_Y	 = {sobelY, 1};		// Sobel filter Y (horizontal)
	constexpr static Filter GAUSS_5	 = {gauss5, 2};		// Harris Gaussian (Ixx, Iyy, Ixy)
	constexpr static Filter GAUSS_1D = {gauss5_1D, 2};	// Harris Gaussian (Ixx, Iyy, Ixy)
	constexpr static Filter NEG_LOG	 = {negLog, 2};		// negative LoG (Marr-Hildreth detector, "Mexican hat")

	struct Keypoint
	{
		int x;
		int y;
		float value;
	};

	/**
	 * @brief Acts as a snapshot onto the renderable RGB or RGBA data of the `ImageProcessor`.
	 * @note used by `PixelStudioApp` for feeding the OpenGL render taxture via GLFW
	 */
	struct ImageBufferView
	{
		std::span<const uint8_t> RGBA;
		uint32_t stamp = 0;
		int width = 0;
		int height = 0;
		int chanCode = 0;
	};


	// ========================================================== TextCode System ==========================================================

	/**
	 * @brief Is holding the identifiers to the predefined constexpr log texts.
	 */
	enum class TextCode : uint8_t
	{
		None = 0,								//   0
		// ------- GLAD & GLFW -------
		GLFW_Init_Failed,						//   1
		GLFW_Window_Failed,						//   2
		GLAD_Init_Failed,						//   3
		// --------- SysInfo ---------
		SYS_Save_Path,							//   4
		SYS_OS_Visual_Mode,						//   5
		SYS_OS_Unknown_Dark_Mode,				//   6
		SYS_OMP_Diag_Results,					//   7
		SYS_OMP_Active,							//   8
		SYS_OMP_RAW_Version,					//   9
		SYS_OMP_V5_Plus,						//  10
		SYS_OMP_V4_5,							//  11
		SYS_OMP_V4_0,							//  12
		SYS_OMP_Legacy,							//  13
		SYS_OMP_Available_Threads,				//  14
		SYS_OMP_Inactive,						//  15
		SYS_OMP_Uninstalled,					//  16
		// --- ImageProcessor (IP) ---
		IP_Init,								//  17
		IP_MAX_THREADS,							//  18
		IP_CHUNK_SIZE,							//  19
		IP_RGBA_resize,							//  20
		IP_Load_IDX,							//  21
		IP_stb_data_Failed,						//  22
		IP_Save_IDX,							//  23
		IP_stb_write_Failed,					//  24
		IP_Delete_IDX,							//  25
		IP_Select_IDX,							//  26
		IP_Skip_Intensity,						//  27
		IP_Skip_Contrast_NoOp,					//  28
		IP_Skip_Contrast_OOR,					//  29
		IP_Posterize_OOR,						//  30
		IP_Pixel_Diff,							//  31
		IP_Skip_Padding,						//  32
		IP_Skip_PixelComp_Size,					//  33
		IP_Skip_PixelComp_NotAvailable,			//  34
		IP_Skip_Alpha0,							//  35
		IP_PDFsum_CDF255,						//  36
		IP_BHT,									//  37
		IP_Alpha_OOR,							//  38
		// --------- Metrics ---------
		Metrics,								//  39
		Metrics_Parallel,						//  40
		Metrics_Comp,							//  41
		// --------- Popups ----------
		SysDiag_Header,							//  42
		SysDiag_Footer,							//  43
		ErrMsg_Header,							//  44
		ErrMsg_Footer,							//  45
		// ---------------------------
		COUNT									//  46 = size of the enum
	};

	/**
	 * @brief Conatiner for the predefined formatted texts and their arguments.
	 */
	struct LogEntry
	{
		TextCode code = TextCode::None;
		std::vector<LogArg> args;
	};

	/**
	 * @brief A container for a series of log entries as well as a separate `bool` status. Since the `Result` type can gather a wide range of information
	 * within a pipeline, the success `bool` value serves to represent the distinct - and respectively most significant - states of the individual steps.
	 */
	struct Result
	{
		bool success = false;
		std::vector<LogEntry> logs;
	};

	/**
	 * @brief Central text table (zero allocation via constexpr & string_view)
	 */
	constexpr std::array<std::string_view, static_cast<size_t>(TextCode::COUNT)> STATUS_TEXTS =
	{
		"",																			//   0
		// ------------------------ GLFW & GLAD -------------------------
		"ERROR: GLFW couldn't be initialized!\n",									//   1
		"ERROR: GLFW window couldn't be created!\n",								//   2
		"ERROR: GLAD couldn't be initialized!\n",									//   3
		// -------------------------- SysInfo ---------------------------
		"successfully created the default output directory!\n",						//   4
		"Detected OS Visual Mode: {}.\n",											//   5
		"Couln't detect a specific OS. Fallback Visual Mode: Dark Mode.\n",			//   6
		"[SysInfo] OpenMP-Diagnosis results:\n",									//   7
		"-> SUCCESS: OpenMP is installed and activated!\n",							//   8
		"-> Detected [RAW] version: {}\n",											//   9
		"-> Detected specification: OpenMP 5.0 or higher\n",						//  10
		"-> Detected specification: OpenMP 4.5\n",									//  11
		"-> Detected specification: OpenMP 4.0\n",									//  12
		"-> Detected specification: legacy OpenMP version (< 4.0)\n",				//  13
		"-> Available CPU-threads for OpenMP: {:3}\n",								//  14
		"-> WARNING: OpenMP (omp.h) is detected on your system,\n"
		"   but the program was compiled /wo the compiler-flag!\n"
		"   Please insert '-fopenmp' (Clang/GCC) to your build-flags.\n"			// or '/openmp' (MSVC)
		"   The program currently runs in single-threaded-mode.\n",					//  15
		"-> DISCLAIMER: OpenMP is not found on your system!\n"
		"   The program runs safely in single-threaded-mode.\n",					//  16
		// --------------------- ImageProcessor (IP) ---------------------
		"[ImageProcessor] initialization results:\n",								//  17
		"system specific optimal MAX_THREADS: {:3}\n",								//  18
		"system specific optimal  CHUNK_SIZE: {:3}\n",								//  19
		"RGBA is resized to hold 16K UHD images!\n",								//  20
		">load image IDX: {:4}   -- SUCCESS! --\n",									//  21
		"loading image ->  -- FAILED! --\npath : \"{}\"\nerror: {}\n",				//  22
		">save image IDX: {:4}   -- SUCCESS! --\n",									//  23
		"saving image ->  -- FAILED! --\npath : \"{}\"\nerror: {}\n",				//  24
		"> del image IDX: {:4}   -- SUCCESS! --\n",									//  25
		"> sel image IDX: {:4}\n",													//  26
		"intensity manip. skipped!\n(value must be within [-1.0, 1.0] \\{0})\n",	//  27
		"contrast manip. skipped!\n(k = 1 changes nothing)\n",						//  28
		"contrast manip. skipped!\n(value must be within [-1, 5])\n",				//  29
		"posterization skipped!\n(range must be 1 <= exp <= 7)\n",					//  30
		"num of \u0394 pixels: {:9}\n",												//  31
		"skipped the image padding!\n(image & filter width -> unchanged!)\n",		//  32
		"compare images skipped!\n(referred image has a different size)\n",			//  33
		"compare images skipped!\n(referred image is not available)\n",				//  34
		"skipped the operation!\n(the image is fully transparent)\n",				//  35
		"sum PDF: {:8.6f} | CDF[255]: {:8.6f}\n",									//  36
		"min = {:3}, max = {:3}, cen = {:3}, CDF[cen] = t = {}\n",					//  37
		"Alpha manip. skipped!\n(value must be within [0.0, 1.0])\n",				//  38
		// -------------------------- Metrics ----------------------------
		"\u0394t ({}): {:7.2f} ms\n", 												//  39
		"\u0394t ({}): {:7.2f} ms (parallel)\n",									//  40
		"\u0394t (comp {:02}:{:02}): {:7.2f} ms\n",									//  41
		// ----------------------- Popup Windows -------------------------
		"System Diagnostics",														//  42
		"Start Pixel Studio",														//  43
		"Error Message",															//  44
		"OK"																		//  45
		// ---------------------------------------------------------------
		// COUNT																	//  46
	};

	/**
	 * @brief Helper to get the identified text comfortable
	 * @param code `TextCode` enum unit8_t 0 to 43
	 * @return the identified text as a `std::string_view`.
	 */
	constexpr std::string_view getText(TextCode code) { return STATUS_TEXTS[static_cast<size_t>(code)]; }

	/**
	 * @brief Helper to get the identified text comfortable and directly as string
	 * @param code `TextCode` enum unit8_t 0 to 43
	 * @return the identified text as a `std::string`.
	 */
	constexpr std::string getAsString(TextCode code) { return static_cast<std::string>(getText(code)); }

	/**
	 * @brief Helper to format the passed args accordingly to the identified text by the `TextCode` enum.
	 * @param code `TextCode` enum unit8_t 0 to 43
	 * @param args alias `LogArg` -> a variant of int, double or string
	 * @return the proper formatted text resulting by the `TextCode` and the parameters
	 */
	inline std::string formatEntry(TextCode code, const std::vector<LogArg>& args)
	{
		std::string_view fmt = getText(code);

		if (args.empty())
			return std::string(fmt);

		switch (args.size())
		{
			case 1:
				return std::visit([fmt](const auto& a0) {
					return std::vformat(fmt, std::make_format_args(a0));
				}, args[0]);

			case 2:
				return std::visit([fmt](const auto& a0, const auto& a1) {
					return std::vformat(fmt, std::make_format_args(a0, a1));
				}, args[0], args[1]);

			case 3:
				return std::visit([fmt](const auto& a0, const auto& a1, const auto& a2) {
					return std::vformat(fmt, std::make_format_args(a0, a1, a2));
				}, args[0], args[1], args[2]);

			case 4:
				return std::visit([fmt](const auto& a0, const auto& a1, const auto& a2, const auto& a3) {
					return std::vformat(fmt, std::make_format_args(a0, a1, a2, a3));
				}, args[0], args[1], args[2], args[3]);

			default:
				return std::string(fmt);
		}
	}
}

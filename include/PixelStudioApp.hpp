#pragma once

#include "Types.hpp"
#include "UIComponents.hpp"
#include "ImageProcessor.hpp"

#include <vector>
#include <string>

#ifndef NOMINMAX																			// preventing possible conflicts by min and max functions
#define NOMINMAX
#endif

// ==============================================================
// Order is important: GLFW before ImGui OpenGL backend!
// ==============================================================
#define GLFW_INCLUDE_NONE																	// prevents GLFW from pulling in standard OpenGL headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "ImGui/imgui.h"

// ==============================================================
// OpenGL Extension Enums for Memory Queries
// ==============================================================
// NVIDIA
#ifndef GL_NVX_gpu_memory_info
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049
#endif
// AMD/ATI
#ifndef GL_VBO_FREE_MEMORY_ATI
#define GL_VBO_FREE_MEMORY_ATI 0x87FB
#endif


// forward declarations
struct GLFWwindow;
struct ImFont;



namespace PixelStudio
{
	/**
	 * @brief Provides an immediate GUI environment for this app and uses an `ImageProcessor` instance to process the heavy weight image processing.
	 * @note Uses externally the "Dear ImGui" library by [Omar Cornut](https://github.com/ocornut/imgui) for realizing an immediate GUI environment.
	 * @note Uses also externally the "stb" library by [Sean Barrett](https://github.com/nothings/stb) just for loading the icons for this app.
	 */
	class PixelStudioApp
	{
	public:
		PixelStudioApp() : m_inspectionData(*this) {}
		~PixelStudioApp() = default;

		int run();


	private:
		inline static constexpr float INV_1024	  = 1.0f / 1024.0f;									// Base constants for memory conversions
		inline static constexpr float BYTES_TO_KB = INV_1024;										// 1D: Bytes -> Kilobytes (e.g., for buffers or line lengths)
		inline static constexpr float BYTES_TO_MB = INV_1024 * INV_1024;							// 2D: Bytes -> Megabytes (e.g., for 2D textures & FBOs)

		inline static uint32_t g_tabID = 0;															// global tabID counter

		/**
		 * @brief GPU vendor identier
		 */
		enum GPU_Vendor : uint8_t { UNKNOWN, NVIDIA, AMD, Intel };

		/**
		 * @brief The mode of VRAM access
		 */
		enum VRAM : uint8_t { UPDATE_AVAIL_VRAM, ALLOC, DEALLOC };

		/**
		 * @brief GPU Information Struct & Detection
		 */
		struct GPUInfo
		{
			std::string vendorStr	= "Unknown";
			std::string rendererStr = "Unknown";
			float used_VRAM	  = 0.0f;
			float avail_VRAM  = 0.0f;
			float total_VRAM  = 0.0f;
			GPU_Vendor vendor = GPU_Vendor::UNKNOWN;
		};

		/**
		 * @brief ImGui Slider settings of an image
		 */
		struct SelectorSettings
		{
			float addIntensity	= 0.00f;
			float sclIntensity	= 0.00f;
			float contrast		= 1.00f;
			float alpha			= 1.00f;
			float threshold		= 0.50f;
			float harr_Sigma	= 1.00f;
			float harr_kFac		= 0.05f;
			float harr_Thresh	= 1.00f;
			float harr_ColorHue = 0.00f;

			uint8_t exp			= 4;
			bool keepCol		= false;
			bool transBg		= false;
			bool keepCol_Auto	= false;
			bool transBg_Auto	= false;
			bool harr_Keypoints	= true;
		};

		/**
		 * @brief Holds all necessary data per image/TAB
		 */
		struct ImageTab
		{
			SelectorSettings settings;
			fs::path srcPath;
			fs::path dstPath;
			std::string fName;
			std::string label;
			uint32_t tabID = 0;
			GLuint texID   = 0;
			uint32_t stamp = 0;
			int width	   = 0;
			int height	   = 0;
			int chanCode   = 0;
		};

		/**
		 * @brief Holds all necessary data of the Feature Detections
		 */
		struct InspectionData
		{
			// the constructor requests the parent app instance
			explicit InspectionData(PixelStudioApp& parent) : app(parent) {}
			PixelStudioApp& app;

			std::vector<Keypoint> keypoints;								// Harris Keypoints

			GLuint tex_Ix = 0;												// texture ID for  Ix data (convolution result: image * Sobel in X direction)
			GLuint tex_Iy = 0;												// texture ID for  Iy data (convolution result: image * Sobel in X direction)
			GLuint texIxx = 0;												// texture ID for Ixx data (dot product: Ix.Ix)
			GLuint texIyy = 0;												// texture ID for Iyy data (dot product: Iy.Iy)
			GLuint texIxy = 0;												// texture ID for Ixy data (dot product: Ix.Iy)

			int width  = 0;													// width of the inspected image
			int height = 0;													// height of the inspected image

			uint32_t stamp  = 0;											// stamp of the latest "Detection Method"

			void setupHarris(int w, int h);
			void setupSIFT(int w, int h);
			void setupSURF(int w, int h);
			void clear();

			// lazy-fetch methods
			GLuint getOrFetchIx();
			GLuint getOrFetchIy();
			GLuint getOrFetchIxx();
			GLuint getOrFetchIyy();
			GLuint getOrFetchIxy();

			void updateThreshold(Result& res, float threshold);

		private:
			// private helper for the OpneGL upload
			GLuint uploadSingleChannel(std::span<const float> data);
		};


		ImageProcessor m_processor;											// the heavy lifting image processing instance
		InspectionData m_inspectionData;									// all the inspection data (Harris, SIFT, SURF)
		GPUInfo m_GPU;														// GPU Info

		std::vector<ImageTab> m_tabs;										// all images currently available in this session (consistent indices /w ImageProcessor)
		std::string m_currPopupText;										// current shown popup text
		std::string m_currLog;												// current shown text of the Performance Log

		GLFWwindow* m_window = nullptr;										// pointer to the main window

		ImVec2 m_mainContentPos;											// cursor position (top-left) of the full image area (main content)
		ImVec2 m_mainContentSize;											// size of the full main content area
		ImVec2 m_renderPos;													// cursor position (top-left) of the image rendering area
		ImVec2 m_renderSize;												// size of the image rendering area

		GLuint m_activeTexID = 0;											// the ID of the currently active texture (OpenGL, GPU, VRAM)

		int m_IDX = -1;														// index of the current TAB

		bool m_firstFrame		= true;										// is the first frame of ImGui shown? (stutter elimination at start)
		bool m_popupToShow		= false;									// is there any pending popup?
		bool m_showConfigModal	= false;									// is the Config Modal to show?
		bool m_themeChanged		= false;									// is there a pending theme change request?
		bool m_isLogOpen		= false;									// is Performance Log open?
		bool m_isModalAnimating	= false;									// is the modal dimming currently running?

		UI::ThemeMode m_currTheme = UI::ThemeMode::DARK;					// current selected theme mode
		UI::ThemeMode m_nextTheme = UI::ThemeMode::DARK;					// next theme mode to switch to
		TextCode m_header = TextCode::None;									// pending popup header
		TextCode m_footer = TextCode::None;									// pending popup footer


		void init(Result & res);

		void beginFrame();
		void renderUI();
		void endFrame();
		void shutdown();

		void renderPopup();
		void renderConfigModal();
		void renderMenuBar();
		void renderCustomTabBar();
		void renderSelectors();
		void renderImageArea();
		void renderInfoBar();

		void loadClicked();
		void saveClicked();
		void tabSelected(int idx);
		void tabClosed(int idx);

		void setTextureID(ImageTab& tab, const ImageBufferView& buff);
		void setNextLog(const Result& res);
		void updateBuffer(const ImageBufferView& buff);
		void setNextPopup(const Result& res, TextCode header, TextCode footer);

		void queryGPUInfo();
		void updateAvailVRAM(VRAM access_mode, int w = 0, int h = 0);
		[[nodiscard]] constexpr float toMB(const int w, const int h) noexcept;
	};
}

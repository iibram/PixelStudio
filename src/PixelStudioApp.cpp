#include "PixelStudioApp.hpp"
#include "SysInfo.hpp"
#include "Types.hpp"

#include "stb/stb_image.h"
#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_opengl3.h"

#include <iostream>
#include <locale>



namespace PixelStudio
{
	// =======================================================================================================================================================================
	// ========================================================================   P  U  B  L  I  C   =========================================================================
	// =======================================================================================================================================================================

	/**
	 * @brief Starts the "Pixel Studio" app and its immediate GUI, initializes the full environment and runs the rendering loop until the app is closed.
	 * @return -1, 0 or 1 for the main() function
	 */
	int PixelStudioApp::run()
	{
		// ---------------------------   S Y S T E M   ---------------------------
		Result res;
		SysInfo::loadAppConfig(res);
		SysInfo::checkVisualMode(res);
		SysInfo::performOpenMPDiagnosis(res);

		// -----------------------------   G  U  I   -----------------------------
		init(res);

		if (!res.success)
		{
			uint8_t i = res.logs.size() - 1;																	// get last entry (.size() <<< 255 -> uint8_t)
			std::cout << formatEntry(res.logs[i].code, {});														// onto terminal (cause GUI itself has failed)
			return -1;
		}

		// ------------------   I M A G E   P R O C E S S O R   ------------------
		m_processor.init(res);																					// init ImageProcessor

		setNextPopup(res, TextCode::SysDiag_Header, TextCode::SysDiag_Footer);									// set system diagnostics popup at start

		// -------------------   R E N D E R I N G   L O O P   -------------------
		while (!glfwWindowShouldClose(m_window))																// as long is running
		{
			beginFrame();																						// poll events & start new frame
			renderUI();																							// set all components of a frame
			endFrame();																							// ImGui::Render & glfwSwapBuffers
		}

		shutdown();																								// shutdown properly when rendering loop stops

		return 0;
	}



	// =======================================================================================================================================================================
	// ======================================================================   P  R  I  V  A  T  E   ========================================================================
	// =======================================================================================================================================================================

	// =======================================================================================================================================================================
	// -----------------------------------------------------------  The Essential GUI Pipeline (+ Initialization)  -----------------------------------------------------------
	// =======================================================================================================================================================================

	/**
	 * @brief Initializes the main window according to the passed parameters properly for a GLFW supported ImGui application.
	 * @param width the initial width of the main window
	 * @param height the initial height of the main window
	 * @param title the title for the main window
	 * @param res `Result` type holding `TextCode` enum indexed constexpr output texts besides a success bool
	 */
	void PixelStudioApp::init(Result & res)
	{
		m_currTheme = res.success ? UI::ThemeMode::DARK : UI::ThemeMode::LYTE;									// success bool was used to identify the OS theme
		res.success = false;																					// reset for next indications

		// ==================================================================== GLFW Setup ===========================================================================
		if (!glfwInit())
		{
			res.logs.push_back({TextCode::GLFW_Init_Failed, {}});												// success = false
			return;
		}

		// Request OpenGL 3.3 Core Profile
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();													// get the dimensions of the primary display
		float xScale = 1.0f, yScale = 1.0f;

		if (primaryMonitor)
			glfwGetMonitorContentScale(primaryMonitor, &xScale, &yScale);										// get the DX / DPI scaling

		UI::em *= yScale;																						// DPI scaled `BaseFont` size

		// start window resolution DPI scaled by the running specific display
		UI::SCALED_W = static_cast<int>(UI::MIN_WIDTH * xScale);
		UI::SCALED_H = static_cast<int>(UI::MIN_HEIGHT * yScale);

		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);																// generate the window invisible
		m_window = glfwCreateWindow(UI::SCALED_W, UI::SCALED_H, UI::APP_NAME, nullptr, nullptr);

		if (!m_window)
		{
			glfwTerminate();

			res.logs.push_back({TextCode::GLFW_Window_Failed, {}});												// success = false
			return;
		}

		// --------------------------------- app icon ---------------------------------
		GLFWimage icons[3];
		int channels = 0;

		icons[0].pixels = stbi_load("res/assets/icons/icon_256.png", &icons[0].width, &icons[0].height, &channels, 4);
		icons[1].pixels = stbi_load("res/assets/icons/icon_32.png", &icons[1].width, &icons[1].height, &channels, 4);
		icons[2].pixels = stbi_load("res/assets/icons/icon_16.png", &icons[2].width, &icons[2].height, &channels, 4);

		if (icons[0].pixels && icons[1].pixels && icons[2].pixels)
		{
			glfwSetWindowIcon(m_window, 3, icons);

			stbi_image_free(icons[0].pixels);
			stbi_image_free(icons[1].pixels);
			stbi_image_free(icons[2].pixels);
		}
		// ----------------------------------------------------------------------------

		// hard constraints (set min. size 1280 x 720 at a FullHD resolution)
		glfwSetWindowSizeLimits(m_window, UI::SCALED_W, UI::SCALED_H, GLFW_DONT_CARE, GLFW_DONT_CARE);

		if (primaryMonitor)
		{
			const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

			if (mode)
			{
				int xpos = ((mode->width - UI::SCALED_W) >> 1);													// calculate the center at x axis
				int ypos = ((mode->height - UI::SCALED_H) >> 1);												// calculate the center at y axis

				glfwSetWindowPos(m_window, xpos, ypos);															// drop the window centered on the screen
			}
		}

		glfwMakeContextCurrent(m_window);

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			res.logs.push_back({TextCode::GLAD_Init_Failed, {}});												// success is set false
			return;
		}

		glfwSwapInterval(1);																					// 1 = V-Sync /w monitor-FPS (e.g. 60 Hz = 60 FPS)

		// =============================================================== ImGui Setup / Context =====================================================================
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		// linking ImGui /w GLFW & OpenGL backend
		ImGui_ImplGlfw_InitForOpenGL(m_window, true);
		ImGui_ImplOpenGL3_Init("#version 330");

		queryGPUInfo();																							// getting vendor & avail VRAM size

		UI::setFonts(yScale);																					// setting the selected ttf Fonts
		UI::applyTheme(m_currTheme);																			// setting the selected OS specific theme as base

		ImGui::GetStyle().ScaleAllSizes(yScale);																// scaling ImGui internal layout/paddings
		UI::scaleDimensions();																					// scaling the apps custom design

		// dynamically clear the first frame (stutter elimination)
		ImVec4 currBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		glClearColor(currBg.x, currBg.y, currBg.z, currBg.w);
		glClear(GL_COLOR_BUFFER_BIT);

		res.success = true;
	}

	/**
	 * @brief Each frame starts here and also pending actions are overtaken here. If there is no any action (incl. user), UI is waiting here (*)
	 */
	void PixelStudioApp::beginFrame()
	{
		if (m_themeChanged)																						// check & poll theme change events
		{
			m_currTheme = m_nextTheme;
			UI::applyTheme(m_currTheme);
			m_themeChanged = false;
		}

		// ------------------------------------------------------------------------
		// GLOBAL MODAL ANIMATION DETECTION:
		// Reads the ImGui fade state universally for EVERY popup/modal in the app.
		// As long as ImGui is fading (0.0 < Ratio < 1.0), bypass glfwWaitEvents()!
		// ------------------------------------------------------------------------
		if (GImGui)
		{
			float dimBgRatio   = GImGui->DimBgRatio;
			m_isModalAnimating = (dimBgRatio > 0.0f && dimBgRatio < 1.0f);
		}

		if (!m_isModalAnimating)
			glfwWaitEvents();																					// (*): FPS only at actions (waiting barrier)
		// ------------------------------------------------------------------------

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	/**
	 * @brief Renders the whole UI (by invoking sub functions) which are to show on the screen at the end of the frame.
	 */
	void PixelStudioApp::renderUI()
	{
		// ==================================================================== Main Window ==========================================================================

		const ImGuiViewport* viewport = ImGui::GetMainViewport();												// using main viewport as base for the app window
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar													// borderless main window for the whole screen
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoCollapse
			| ImGuiWindowFlags_NoBringToFrontOnFocus;

		// ==================================================================== ImGui begin ==========================================================================

		ImGui::Begin("PixelStudioMain", nullptr, windowFlags);

		renderPopup();
		renderMenuBar();
		renderConfigModal();
		renderCustomTabBar();
		renderSelectors();
		renderImageArea();
		renderInfoBar();

		ImGui::End();

		// ===================================================================== ImGui end ===========================================================================
	}

	/**
	 * @brief Each frame ends here. The actual graphical rendering by the GPU happens here.
	 * Additionally, this function houses synchronizations and the available VRAM tracking feature, since the necessary acquisition of display dimensions
	 * takes place here anyway.
	 */
	void PixelStudioApp::endFrame()
	{
		ImGui::Render();																						// the ACTUAL rendering

		// setting framebuffer size & viewport (since resizing is enabled)
		int display_w = 0, display_h = 0;
		glfwGetFramebufferSize(m_window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);

		// ----------------------------------------------------------------------------
		//  			VRAM Window-Resize Delta Tracking (Zero-Overhead)
		// ----------------------------------------------------------------------------
		static int last_w = display_w;
		static int last_h = display_h;

		// lambda helper: Exact framebuffer size in MB
		auto calcFramebufferVRAM = [](int w, int h) -> float {
			constexpr float BYTES_PER_PIXEL = 8.0f;
			constexpr float BUFFER_COUNT    = 2.0f; 															// Front & Back Buffer
			return (static_cast<float>(w * h) * BYTES_PER_PIXEL * BUFFER_COUNT) * BYTES_TO_MB;
		};

		// from frame > 1 --> start calculating window resolution affect on VRAM usage
		if (!m_firstFrame)
		{
			if (display_w != last_w || display_h != last_h)
			{
				// calculate exact VRAM usage for OLD and NEW size
				float old_fb_vram = calcFramebufferVRAM(last_w, last_h);
				float new_fb_vram = calcFramebufferVRAM(display_w, display_h);

				// extract the delta
				float vram_delta = new_fb_vram - old_fb_vram;

				// adjust VRAM budget
				m_GPU.avail_VRAM -= vram_delta;

				last_w = display_w;
				last_h = display_h;
			}
		}
		// ----------------------------------------------------------------------------

		// dynamically clearing the background /w the custom ImGui theme
		ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		glClearColor(bg.x, bg.y, bg.z, bg.w);
		glClear(GL_COLOR_BUFFER_BIT);

		// ImGui geometry, rendering and buffer swap
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(m_window);

		// ----------------------------------------------------------------------------
		// 					  Startup Initialization (Frame 1 Sync)
		// ----------------------------------------------------------------------------
		if (m_firstFrame)
		{
			glfwShowWindow(m_window);																			// show synced first frame

			updateAvailVRAM(VRAM::UPDATE_AVAIL_VRAM);
			m_GPU.avail_VRAM -= 2.0f;																			// 2 MB baseline adjustment
			m_GPU.total_VRAM = m_GPU.avail_VRAM;																// synchronize

			// Set baseline for resize monitoring exactly to the initial framebuffer
			last_w = display_w;
			last_h = display_h;

			m_firstFrame = false;
		}
	}

	/**
	 * @brief Cleans up and shutdowns the UI properly
	 */
	void PixelStudioApp::shutdown()
	{
		// delete existing inspection data
		m_inspectionData.clear();

		// delete existing image textures
		for (auto& tab : m_tabs)
		{
			if (tab.texID != 0)
			{
				glDeleteTextures(1, &tab.texID);
				tab.texID = 0;
			}
		}

		// shutdown ImGui
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		// shutdown GLFW & OpenGL-context
		glfwDestroyWindow(m_window);
		glfwTerminate();
	}


	// =======================================================================================================================================================================
	// ---------------------------------------------------------------  Performing the GUI pipeline (Widgets)  ---------------------------------------------------------------
	// =======================================================================================================================================================================

	/**
	 * @brief Shows the next popup window (center of main window) with important data, such as the essential system environments for the app or failures.
	 */
	void PixelStudioApp::renderPopup()
	{
		// ------------------------------------------------------------------------ Popup ----------------------------------------------------------------------------

		if (!m_popupToShow) return;

		std::string header = getAsString(m_header);
		ImGui::OpenPopup(header.c_str());

		// modal centering & rendering
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(header.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::PushFont(UI::InfoFont);
			ImGui::TextUnformatted(m_currPopupText.c_str());
			ImGui::PopFont();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button(getAsString(m_footer).c_str(), ImVec2(-1.0f, 0.0f)))
			{
				m_popupToShow = false;
				m_currPopupText.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	/**
	 * @brief
	 */
	void PixelStudioApp::renderConfigModal()
	{
		// -------------------------------------------------------------------- Config Modal -------------------------------------------------------------------------

		if (!m_showConfigModal) return;

		ImGui::SetNextWindowPos(UI::ConfigPos, ImGuiCond_Appearing);											// position upper left under the MenuBar
		const char* popupTitle = "Preferences / Paths";

		ImGui::OpenPopup(popupTitle);

		if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Application Default Paths");
			ImGui::Separator();
			ImGui::Spacing();

			// -------------------- DEFAULT LOAD PATH --------------------
			std::string loadStr = PixelStudio::DEFAULT_LOAD_PATH.string();
			ImGui::Text("Default Load Path:");

			ImGui::SetNextItemWidth(UI::PathLen);
			ImGui::InputText("##loadpath", &loadStr[0], loadStr.size(), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();

			if (ImGui::Button("Browse##load"))
			{
				fs::path selected = PixelStudio::SysInfo::selectFolderDialog("Select Default Load Directory");
				if (!selected.empty() && fs::is_directory(selected))
				{
					PixelStudio::DEFAULT_LOAD_PATH = selected;
					PixelStudio::SysInfo::saveAppConfig();
				}
			}

			ImGui::Spacing();

			// -------------------- DEFAULT SAVE PATH --------------------
			std::string saveStr = PixelStudio::DEFAULT_SAVE_PATH.string();
			ImGui::Text("Default Save Path:");

			ImGui::SetNextItemWidth(UI::PathLen);
			ImGui::InputText("##savepath", &saveStr[0], saveStr.size(), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();

			if (ImGui::Button("Browse##save"))
			{
				fs::path selected = PixelStudio::SysInfo::selectFolderDialog("Select Default Save Directory");
				if (!selected.empty() && fs::is_directory(selected))
				{
					PixelStudio::DEFAULT_SAVE_PATH = selected;
					PixelStudio::SysInfo::saveAppConfig();
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// ------------------- FOOTER -------------------
			if (ImGui::Button("Close", ImVec2(-1.0f, 0.0f)))
			{
				m_showConfigModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	/**
	 * @brief Realizes the Menu Bar and its options
	 */
	void PixelStudioApp::renderMenuBar()
	{
		// ----------------------------------------------------------------------- Menu Bar --------------------------------------------------------------------------

		ImVec4 textCol = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Text)).Value;								// get mode based text color
		textCol.w = 0.7f;																						// reduce Alpha to 70%

		ImGui::PushStyleColor(ImGuiCol_Text, textCol);															// push stightly less decent text color

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (UI::CustomMenuItem("Open", "CTRL + O")) { loadClicked(); }
				if (UI::CustomMenuItem("Save", "CTRL + S")) { saveClicked(); }
				ImGui::Separator();
				if (UI::CustomMenuItem("Exit", "CTRL + X")) { glfwSetWindowShouldClose(m_window, true); }

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Tools"))
			{
				if (UI::CustomMenuItem("Config")) { m_showConfigModal = true; }

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				if (UI::CustomMenuItem("About")) { /* TODO */ }

				ImGui::EndMenu();
			}

			// ---------------- integrated Theme Toggle Button (TTB) ------------------
			if (UI::ThemeToggleButton(m_currTheme, m_nextTheme)) m_themeChanged = true;

			ImGui::EndMenuBar();
		}
		ImGui::PopStyleColor();
	}

	/**
	 * @brief Visualizes the TAB representation for each image currently active in the app and that "CustomTabButtons" behavior.
	 */
	void PixelStudioApp::renderCustomTabBar()
	{
		// -------------------------------------------------------------------- Custom TAB Bar -----------------------------------------------------------------------

		if (m_tabs.empty()) ImGui::Dummy(ImVec2(0.0f, UI::em + UI::TabDim.y));

		else
		{
			int closeIDX = -1;

			for (int i = 0; i < m_tabs.size(); ++i)
			{
				ImGui::PushID(static_cast<int>(m_tabs[i].tabID));

				bool isSelected = false;
				const bool isActive = (i == m_IDX);

				if (PixelStudio::UI::CustomTabButton(m_tabs[i].label.c_str(), m_tabs[i].fName.c_str(), isActive, isSelected))
					closeIDX = i;

				if (isSelected && !isActive)																	// only when user selects another TAB
					tabSelected(i);

				ImGui::SameLine(0.0f, UI::Padding.y);															// spacing to next Custom TAB
				ImGui::PopID();
			}
			ImGui::NewLine();

			if (closeIDX != -1)																					// when user closes a TAB
				tabClosed(closeIDX);																			// closing it outside the TAB loop (iterator valid)
		}
	}

	/**
	 * @brief Calcluates and realizes the Selector Area to manipulate the selected image.
	 */
	void PixelStudioApp::renderSelectors()
	{
		// ---------------------------------------------------- Calculation of the full area of the "Selector" -------------------------------------------------------

		m_mainContentSize	 = ImGui::GetContentRegionAvail();									// getting the remaining size (after MenuBar & CustomTabBar)

		m_mainContentSize.x	-= (UI::Selector_W + UI::Padding.x);								// subtract Selector + pad of width
		m_mainContentSize.y	-= UI::InfoBar_H;													// subtract InfoBar from the height	-> mainContentSize (ready) !!!

		m_mainContentSize.x	= std::max(1.0f, m_mainContentSize.x);								// safety guard during resize
		m_mainContentSize.y	= std::max(1.0f, m_mainContentSize.y);								// safety guard during resize

		// -------------------------------------------------------------------- Selector Area ------------------------------------------------------------------------

		const bool hasTabs = !m_tabs.empty();

		static SelectorSettings defaultSettings {};
		auto& s = hasTabs ? m_tabs[m_IDX].settings : defaultSettings;

		const bool isInspectionActive = hasTabs && (m_inspectionData.stamp != 0 && m_inspectionData.stamp == m_tabs[m_IDX].stamp);

		const GLuint tex_Ix	= m_inspectionData.tex_Ix;
		const GLuint tex_Iy	= m_inspectionData.tex_Iy;
		const GLuint texIxx	= m_inspectionData.texIxx;
		const GLuint texIyy	= m_inspectionData.texIyy;
		const GLuint texIxy	= m_inspectionData.texIxy;

		const GLuint origTexID = hasTabs ? m_tabs[m_IDX].texID : 0;
		const bool isIxActive  = (tex_Ix != 0 && m_activeTexID == tex_Ix);
		const bool isIyActive  = (tex_Iy != 0 && m_activeTexID == tex_Iy);
		const bool isIxxActive = (texIxx != 0 && m_activeTexID == texIxx);
		const bool isIyyActive = (texIyy != 0 && m_activeTexID == texIyy);
		const bool isIxyActive = (texIxy != 0 && m_activeTexID == texIxy);


		ImGuiWindowFlags noScrollbar = ImGuiWindowFlags_NoScrollbar;
		ImGuiSliderFlags noInput	 = ImGuiSliderFlags_NoInput;
		ImGuiSliderFlags log		 = ImGuiSliderFlags_Logarithmic;

		ImGui::BeginChild("SelectorPanel", ImVec2(UI::Selector_W, m_mainContentSize.y), true, noScrollbar);
		{
			// calculate the SelectorPanel positioning of the TOP SECTION and LOG SECTION
			const float collapsedLogHeight = UI::em + UI::Padding.x;
			const float reservedLogHeight  = m_isLogOpen ? UI::OpenLog_H : collapsedLogHeight;

			float topH = ImGui::GetContentRegionAvail().y - UI::Padding.y - reservedLogHeight;
			const float topHeight	= std::max(1.0f, topH);
			const float fullAvail_W = ImGui::GetContentRegionAvail().x;

			const ImVec2 fullBtnDim = ImVec2(fullAvail_W, 0.0f);
			ImVec2 applyBtnDim;																					// will be set by the first apply button (*)

			// =============================================================================================================================================
			// 														T O P   C O N T R O L   S E C T I O N
			// =============================================================================================================================================
			ImGui::BeginChild("TopControlSection", ImVec2(0, topHeight), false, noScrollbar);

			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(m_tabs.empty());

			// ====================================================================================================
			// 										  > GLOBAL MANIPULATIONS
			// ====================================================================================================
			if (ImGui::CollapsingHeader("Global Manipulations"))
			{
				// static slider min max vals
				static uint8_t expMin = 1;
				static uint8_t expMax = 7;

				ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;

				// -----------------------------------  ADD INTENSITY  ------------------------------------
				if (ImGui::TreeNodeEx("Intesity (additive)##Node", treeFlags))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);
					ImGui::SetNextItemWidth(UI::SliderA_W);
					ImGui::SliderFloat("##AddIntesitySlider", &s.addIntensity, -1.0f, 1.0f, "%.1f", noInput);
					ImGui::PopStyleVar();

					ImGui::SameLine();
					applyBtnDim  = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);								// set applyBtnDim dynamically (right end) (*)

					if (ImGui::Button("apply##AddIntensity", applyBtnDim))
					{
						Result res = m_processor.addIntensity(s.addIntensity);
						setNextLog(res);
						if (res.success)
							updateBuffer(m_processor.getImageBufferView());
					}
					ImGui::Spacing();
				}

				// ----------------------------------  SCALE INTENSITY  -----------------------------------
				if (ImGui::TreeNodeEx("Intesity (scaled)##Node", treeFlags))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);
					ImGui::SetNextItemWidth(UI::SliderA_W);
					ImGui::SliderFloat("##ScaleIntesitySlider", &s.sclIntensity, -1.0f, 1.0f, "%.1f", noInput);
					ImGui::PopStyleVar();

					ImGui::SameLine();
					if (ImGui::Button("apply##ScaleIntensity", applyBtnDim))
					{
						Result res = m_processor.scaleIntensity(s.sclIntensity);
						setNextLog(res);
						if (res.success)
							updateBuffer(m_processor.getImageBufferView());
					}
					ImGui::Spacing();
				}

				// -------------------------------------  CONTRAST  ---------------------------------------
				if (ImGui::TreeNodeEx("Contrast##Node", treeFlags))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);
					ImGui::SetNextItemWidth(UI::SliderA_W);
					ImGui::SliderFloat("##ContrastSlider", &s.contrast, 0.0f, 3.0f, "%.1f", noInput);
					ImGui::PopStyleVar();

					ImGui::SameLine();
					if (ImGui::Button("apply##Contrast", applyBtnDim))
					{
						Result res = m_processor.setContrast(s.contrast);
						setNextLog(res);
						if (res.success)
							updateBuffer(m_processor.getImageBufferView());
					}
					ImGui::Spacing();
				}

				// -------------------------------------  POSTERIZE  --------------------------------------
				if (ImGui::TreeNodeEx("Posterize##Node", treeFlags))
				{
					//ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);
					ImGui::SetNextItemWidth(UI::SliderA_W);
					ImGui::SliderScalar("##PosterizeSlider", ImGuiDataType_U8, &s.exp, &expMin, &expMax, "%u", noInput);
					ImGui::PopStyleVar();

					ImGui::SameLine();
					if (ImGui::Button("apply##Posterize", applyBtnDim))
					{
						Result res = m_processor.posterize(s.exp);
						setNextLog(res);
						if (res.success)
							updateBuffer(m_processor.getImageBufferView());
					}
					ImGui::Spacing();
				}

				// -------------------------------------  SET ALPHA  --------------------------------------
				if (ImGui::TreeNodeEx("Set Alpha##Node", treeFlags))
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);
					ImGui::SetNextItemWidth(UI::SliderA_W);
					ImGui::SliderFloat("##SetAlphaSlider", &s.alpha, 0.0f, 1.0f, "%.2f", noInput);
					ImGui::PopStyleVar();

					ImGui::SameLine();
					if (ImGui::Button("apply##SetAlpha", applyBtnDim))
					{
						Result res = m_processor.setAlpha(s.alpha);
						setNextLog(res);
						if (res.success)
							updateBuffer(m_processor.getImageBufferView());
					}
					ImGui::Spacing();
				}

				// ------------------------------------  SEGMENTATION  ------------------------------------
				if (ImGui::TreeNodeEx("Manual Segmentation##Node", treeFlags))
				{
					uint8_t segmType = 0;
					if (s.keepCol) segmType |= FOREGROUND_KEEP_COLOR;
					if (s.transBg) segmType |= BACKGROUND_TRANSPARENT;

					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, UI::CheckVec2);

					ImGui::Indent(UI::Padding.x);
					ImGui::Checkbox("Keep FG Color##Segmentation", &s.keepCol);
					ImGui::SameLine(0.0f, UI::Padding_X2);
					ImGui::Checkbox("Set BG Opacity 0##Segmentation", &s.transBg);
					ImGui::Unindent(UI::Padding.x);
					ImGui::PopStyleVar(); // CheckVec2

					ImGui::SetNextItemWidth(UI::SliderA_W);
					ImGui::SliderFloat("##Segm_Threshold", &s.threshold, 0.0f, 1.0f, "%.2f", noInput);
					ImGui::PopStyleVar(); // FrameSize

					ImGui::SameLine();
					if (ImGui::Button("apply##Segmentation", applyBtnDim))
					{
						Result res;
						res = m_processor.applySegmentation(s.threshold, res, segmType);
						setNextLog(res);
						if (res.success)
							updateBuffer(m_processor.getImageBufferView());
					}
					ImGui::Spacing();
				}

				// ---------------------------------  AUTO SEGMENTATION  ----------------------------------
				if (ImGui::TreeNodeEx("Auto Segmentation##Node", treeFlags))
				{
					uint8_t autoSegmType = 0;
					if (s.keepCol_Auto) autoSegmType |= FOREGROUND_KEEP_COLOR;
					if (s.transBg_Auto) autoSegmType |= BACKGROUND_TRANSPARENT;

					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, UI::CheckVec2);
					ImGui::Indent(UI::Padding.x);
					ImGui::Checkbox("Keep FG Color##AutoSegm", &s.keepCol_Auto);
					ImGui::SameLine(0.0f, UI::Padding_X2);
					ImGui::Checkbox("Set BG Opacity 0##AutoSegm", &s.transBg_Auto);
					ImGui::Unindent(UI::Padding.x);
					ImGui::PopStyleVar(2);

					if (ImGui::Button("apply Auto Segmentation", fullBtnDim))
					{
						Result res = m_processor.applyAutoSegmentation(autoSegmType);
						setNextLog(res);
						if (res.success)
							updateBuffer(m_processor.getImageBufferView());
					}
					ImGui::Spacing();
				}

				// -------------------------------------  NEGATIVE  ---------------------------------------
				if (ImGui::Button("Invert (Negative)", fullBtnDim))
				{
					Result res = m_processor.toNegative();
					setNextLog(res);
					if (res.success)
						updateBuffer(m_processor.getImageBufferView());

					ImGui::Spacing();
				}

				// -----------------------------------  AUTO HISTOGRAM  -----------------------------------
				if (ImGui::Button("Auto Histogram Equalization", fullBtnDim))
				{
					Result res = m_processor.applyHistogramEqualization();
					setNextLog(res);
					if (res.success)
						updateBuffer(m_processor.getImageBufferView());

					ImGui::Spacing();
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// ====================================================================================================
			// 										  > LOCAL MANIPULATIONS
			// ====================================================================================================
			if (ImGui::CollapsingHeader("Local Manipulations"))
			{
				float spacing  = ImGui::GetStyle().ItemSpacing.x;
				float colWidth = std::floor((fullAvail_W - spacing) * 0.5f);
				const ImVec2 leftDim = ImVec2(colWidth, 0.0f);
				ImVec2 rightDim;																				// will be set by the first right inspection btn (*)

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// -------------------------------------------------------------------------------------------
				// 								  HARRIS-STEPHENS CORNER DETECTOR
				// -------------------------------------------------------------------------------------------
				if (ImGui::CollapsingHeader("Harris-Stephens Corner Detector"))
				{
					// ------------------------------------------------------------------------------------
					ImGui::TextDisabled("Derivatives & Inspection:");
					// ------------------------------------------------------------------------------------
					ImGui::BeginDisabled(!isInspectionActive);

					if (UI::ToggleButton("Show Ix", isIxActive, leftDim))
						m_activeTexID = isIxActive ? origTexID : m_inspectionData.getOrFetchIx();
					ImGui::SameLine();
					rightDim = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);									// set right inspection btn width dynamically (*)
					if (UI::ToggleButton("Show Iy", isIyActive, rightDim))
						m_activeTexID = isIyActive ? origTexID : m_inspectionData.getOrFetchIy();

					if (UI::ToggleButton("Show Ixx", isIxxActive, leftDim))
						m_activeTexID = isIxxActive ? origTexID : m_inspectionData.getOrFetchIxx();
					ImGui::SameLine();
					if (UI::ToggleButton("Show Iyy", isIyyActive, rightDim))
						m_activeTexID = isIyyActive ? origTexID : m_inspectionData.getOrFetchIyy();

					if (UI::ToggleButton("Show Ixy", isIxyActive, leftDim))
						m_activeTexID = isIxyActive ? origTexID : m_inspectionData.getOrFetchIxy();
					ImGui::SameLine();
					ImGui::SetNextItemWidth(rightDim.x);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);
					ImGui::SliderFloat("##dot_color", &s.harr_ColorHue, 0.0f, 1.0f, "Color");

					// ------------------------------------------------------------------------------------
					// 									Keypoints Control
					// ------------------------------------------------------------------------------------
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, UI::CheckVec2);
					ImGui::Checkbox("Render Keypoints", &s.harr_Keypoints);

					ImGui::SameLine(); ImGui::TextDisabled(" num pts:"); ImGui::SameLine();
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + UI::Line - ImGui::GetStyle().ItemSpacing.x);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

					ImGui::PushFont(UI::InfoFont);
					ImGui::InputText("##kpts_count", const_cast<char*>(std::format(std::locale(""), "{:>11L}",
									 isInspectionActive ? m_inspectionData.keypoints.size() : 0).c_str()),
									 12, ImGuiInputTextFlags_ReadOnly);
					ImGui::PopFont();

					ImGui::PopStyleVar(2); // Line, CheckVec2
					// ------------------------------------------------------------------------------------

					ImGui::EndDisabled();
					ImGui::Spacing();

					// ------------------------------------------------------------------------------------
					ImGui::TextDisabled("Algorithm Parameters:");
					// ------------------------------------------------------------------------------------
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, UI::Line);

					ImGui::PushItemWidth(UI::SliderB_W);
					ImGui::SliderFloat("Sigma (\u03C3)", &s.harr_Sigma, 0.5f, 5.0f, "%.1f", noInput);

					ImGui::SliderFloat("##kFactor", &s.harr_kFac, 0.01f, 0.10f, "%.3f", noInput);
					if (isInspectionActive && ImGui::IsItemDeactivatedAfterEdit())
					{
						// complete Harris calculation when Harris active & k-factor slider moved !
						m_activeTexID = origTexID;																// swap texID to original image
						Result res = m_processor.applyHarrisXY(s.harr_Sigma, s.harr_kFac, s.harr_Thresh);
						setNextLog(res);

						if (res.success)
							m_inspectionData.setupHarris(m_tabs[m_IDX].width, m_tabs[m_IDX].height);
					}
					ImGui::SameLine(UI::SliderB_W + ImGui::GetStyle().ItemInnerSpacing.x);
					ImGui::AlignTextToFramePadding();
					ImGui::PushFont(UI::MathFont); ImGui::Text("k"); ImGui::PopFont();
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::Text("-Factor");

					// quick new keypoint calculation when Harris active
					ImGui::SliderFloat("threshold", &s.harr_Thresh, 0.05f, 10.0f, "%.2f", noInput | log);
					if (isInspectionActive && ImGui::IsItemDeactivatedAfterEdit())
					{
						Result res;
						m_inspectionData.updateThreshold(res, s.harr_Thresh);
						setNextLog(res);
					}
					ImGui::PopItemWidth();	// SliderB_W
					ImGui::PopStyleVar();	// Line

					ImGui::Spacing();

					// ----------------------- Harris Corner Detection Compute Button ---------------------
					ImGui::BeginDisabled(isInspectionActive);

					if (ImGui::Button("Compute Detection", fullBtnDim))
					{
						Result res = m_processor.applyHarrisXY(s.harr_Sigma, s.harr_kFac, s.harr_Thresh);
						setNextLog(res);

						if (res.success)
							m_inspectionData.setupHarris(m_tabs[m_IDX].width, m_tabs[m_IDX].height);

						ImGui::Spacing();
					}
					ImGui::EndDisabled();
					// ------------------------------------------------------------------------------------

				}
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// -------------------------------------------------------------------------------------------
				// 						SIFT (SCALE-INVARIANT FEATURE TRANSFORM) [UPCOMING]
				// -------------------------------------------------------------------------------------------
				if (ImGui::CollapsingHeader("SIFT (Scale-Invariant Feature Transform)"))
				{
					ImGui::TextDisabled("[Upcoming]");
				}
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// -------------------------------------------------------------------------------------------
				// 							SURF (SPEEDED UP ROBUST FEATURE) [UPCOMING]
				// -------------------------------------------------------------------------------------------
				if (ImGui::CollapsingHeader("SURF (Speeded Up Robust Features)"))
				{
					ImGui::TextDisabled("[Upcoming]");
				}
			}

			ImGui::Spacing();
			ImGui::Separator();

			ImGui::EndDisabled();
			ImGui::EndChild(); // end: TopControlSection


			// =============================================================================================================================================
			// 											  B O T T O M   C O N T R O L   S E C T I O N   (Performance Log)
			// =============================================================================================================================================
			if (ImGui::CollapsingHeader("Performance Log", ImGuiTreeNodeFlags_DefaultOpen))
			{
				m_isLogOpen = true;

				ImGui::PushFont(UI::InfoFont);

				if (m_currLog.empty())
					ImGui::TextDisabled("Ready.");
				else
					ImGui::TextDisabled("%s", m_currLog.c_str());

				ImGui::PopFont();
			}
			else
				m_isLogOpen = false;
		}
		ImGui::EndChild(); // end: SelectorPanel
	}

	/**
	 * @brief Shows the current active image in the center of the "Image Area" and keeps the aspect ratio of that image.
	 */
	void PixelStudioApp::renderImageArea()
	{
		// ---------------------------------------------------- Calculation of the rendering area for images ---------------------------------------------------------

		ImGui::SameLine(); 																		// placing the image area directly to the right of the "Selector Area"

		m_mainContentPos = ImGui::GetCursorScreenPos();          								// top-left pos (0, 0) of the full main content area (incl. frames)

		m_renderPos.x  = UI::Line + UI::Padding.x;												// setting renderPos X
		m_renderPos.y  = UI::Line + UI::Padding.x;												// setting renderPos Y		 		-> renderPos (ready) !!!
		m_renderSize.x = m_mainContentSize.x - (UI::Line_X2 + UI::Padding_X2);					// setting renderSize width
		m_renderSize.y = m_mainContentSize.y - (UI::Line_X2 + UI::Padding_X2);					// setting renderSize height 		-> renderSize (ready) !!!

		// --------------------------------------------------------------------- Image Area --------------------------------------------------------------------------

		ImGui::SetCursorScreenPos(m_mainContentPos);															// pos (0, 0) of the whole "Image Area"

		if (ImGui::BeginChild("ImageRegion", m_mainContentSize, true, ImGuiWindowFlags_NoScrollbar))
		{
			if (m_tabs.empty())
				ImGui::TextDisabled("No image loaded. Use 'File -> Open' to open an image.");

			else
			{
				ImageTab& tab = m_tabs[m_IDX];

				if (m_activeTexID != 0 && tab.width > 0 && tab.height > 0)
				{
					float img_w = static_cast<float>(tab.width), img_h = static_cast<float>(tab.height);

					float imageAspect  = img_w / img_h;
					float regionAspect = m_renderSize.x / m_renderSize.y;

					ImVec2 finalSize;
					if (regionAspect > imageAspect)
					{
						finalSize.y = m_renderSize.y;
						finalSize.x = m_renderSize.y * imageAspect;
					}
					else
					{
						finalSize.x = m_renderSize.x;
						finalSize.y = m_renderSize.x / imageAspect;
					}

					// offset inside the proper region
					float offsetX = (m_renderSize.x - finalSize.x) * 0.5f;
					float offsetY = (m_renderSize.y - finalSize.y) * 0.5f;

					ImGui::SetCursorPos(ImVec2(m_renderPos.x + offsetX, m_renderPos.y + offsetY));				// set the offset onto the pre-calculated renderPos

					ImVec2 imgStartPos = ImGui::GetCursorScreenPos();											// save image start position and
					float imgScale	   = finalSize.x / img_w;													// the scale for KEYPOINT OVERLAY

					ImGui::Image((ImTextureID)(uintptr_t)m_activeTexID, finalSize);								// render the image

					// ----------------------------------------- KEYPOINT OVERLAY PASS --------------------------------------------
					if (m_inspectionData.stamp == tab.stamp && !m_inspectionData.keypoints.empty() && tab.settings.harr_Keypoints)
					{
						ImDrawList* drawList  = ImGui::GetWindowDrawList();
						ImVec4 color		  = UI::GetKeypointColor(tab.settings.harr_ColorHue);
						ImU32 dotColor		  = ImGui::ColorConvertFloat4ToU32(color);

						const float halfBoxSize = 2.0f;															// half-size for a perfectly centered 4x4 pixel box

						for (const auto& kp : m_inspectionData.keypoints)
						{
							// hit the pixel center: Add +0.5f to kp.x and kp.y before scaling
							const float centerImageX = static_cast<float>(kp.x) + 0.5f;
							const float centerImageY = static_cast<float>(kp.y) + 0.5f;

							// imgStartPos and imgScale always refer to the currently rendered texture rectangle
							const float screenX = imgStartPos.x + (centerImageX * imgScale);
							const float screenY = imgStartPos.y + (centerImageY * imgScale);

							// Pixel-exact centered 4x4 bounding box around the subpixel center
							drawList->AddRectFilled(
								ImVec2(screenX - halfBoxSize, screenY - halfBoxSize),
								ImVec2(screenX + halfBoxSize, screenY + halfBoxSize),
								dotColor
							);
						}
					}// -----------------------------------------------------------------------------------------------------------
				}
			}
		}
		ImGui::EndChild();
	}

	/**
	 * @brief Builds the Info Bar showing useful informations of the current selected image.
	 */
	void PixelStudioApp::renderInfoBar()
	{
		// ----------------------------------------------------------------------- Info Bar --------------------------------------------------------------------------

		ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::TransparentVec4);

		if (ImGui::BeginChild("InfoRegion", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar))
		{
			ImGui::PushFont(UI::InfoFont);

			float offsetY = (ImGui::GetWindowHeight() - ImGui::GetTextLineHeight()) * 0.5f;
			ImGui::SetCursorPosY(offsetY);

			// -------------------------------------------------------------------------------------
			// left block: image meta data
			// -------------------------------------------------------------------------------------
			if (!m_tabs.empty())
			{
				ImageTab& tab = m_tabs[m_IDX];
				int channels = (tab.chanCode == 3) ? 3 : 4;
				float sizeMB = static_cast<float>(tab.width * tab.height * channels) * BYTES_TO_MB;

				// left block: image meta data
				ImGui::TextDisabled("File: %s  |  %d x %d px  |  RGBA (%d-Bit)  |  %.2f MB (uncompressed) |",
							tab.label.c_str(),
							tab.width,
							tab.height,
							(channels << 3),
							sizeMB);

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", tab.srcPath.generic_string().c_str());

			}
			else
				ImGui::TextDisabled("No active image");

			ImGui::SameLine();

			// -------------------------------------------------------------------------------------
			// append: used VRAM info of this app (colored)
			// -------------------------------------------------------------------------------------
			if (m_GPU.avail_VRAM > 0.0f)
			{
				float ratio = m_GPU.used_VRAM / m_GPU.avail_VRAM;
				ImVec4 statusColor;

				if (ratio < 0.60f)		statusColor = UI::VRAM_GOOD;
				else if (ratio < 0.85f) statusColor = UI::VRAM_WARN;
				else					statusColor = UI::VRAM_ALERT;

				ImGui::TextColored(statusColor, "VRAM used: %.2f / %.2f MB (%s)", m_GPU.used_VRAM, m_GPU.avail_VRAM, m_GPU.rendererStr.c_str());

				if (ImGui::IsItemHovered())
				{
					// show tooltip
					ImGui::BeginTooltip();
					ImGui::Text("Total VRAM @app_start: %.2f MB", m_GPU.total_VRAM);
					ImGui::EndTooltip();
				}
			}
			else
				ImGui::Text("VRAM used: %.2f MB (%s)", m_GPU.used_VRAM, m_GPU.rendererStr.c_str());

			ImGui::PopFont();

			// -------------------------------------------------------------------------------------
			// right block: App version
			// -------------------------------------------------------------------------------------
			float versionWidth = ImGui::CalcTextSize(UI::APP_FULL_TITLE).x;
			float targetX = ImGui::GetWindowWidth() - versionWidth;

			// jump to the right border and set text
			ImGui::SameLine(targetX); ImGui::SetCursorPosY(offsetY + UI::Line);
			ImGui::Text("%s", UI::APP_FULL_TITLE);
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}


	// =======================================================================================================================================================================
	// --------------------------------------------------------------  User Input Controls (the essential ones)  -------------------------------------------------------------
	// =======================================================================================================================================================================

	// ------------------------------------------   L O A D   -------------------------------------------
	void PixelStudioApp::loadClicked()
	{
		fs::path loadPath = (m_tabs.size() == 0) ? DEFAULT_LOAD_PATH : m_tabs[m_IDX].srcPath.parent_path();
		fs::path selectedPath = SysInfo::loadFileDialog(loadPath.string());

		if (selectedPath.empty()) return;

		Result res = m_processor.loadImage(selectedPath);

		if (res.success)
		{
			const ImageBufferView& buffer = m_processor.getImageBufferView();

			std::string fileName = selectedPath.filename().string();
			std::string shortName = (fileName.length() > 18) ? fileName.substr(0, 15) + "..." : fileName;

			if (!buffer.RGBA.empty())
			{
				m_tabs.push_back({
					.srcPath  = selectedPath,
					.fName	  = fileName,
					.label	  = shortName,
					.tabID	  = ++g_tabID,
					.stamp	  = buffer.stamp,
					.width	  = buffer.width,
					.height	  = buffer.height,
					.chanCode = buffer.chanCode});

				m_IDX = static_cast<int>(m_tabs.size()) - 1;

				setTextureID(m_tabs[m_IDX], buffer);
				m_activeTexID = m_tabs[m_IDX].texID;

				if (buffer.chanCode == 0)
					m_tabs[m_IDX].settings.alpha = 0.0f;
				else if (buffer.chanCode == 3)
					m_tabs[m_IDX].settings.alpha = 1.0f;
				else
					m_tabs[m_IDX].settings.alpha = 0.5f;

				updateAvailVRAM(VRAM::ALLOC, buffer.width, buffer.height);

				setNextLog(res);
			}
		}
		else
			setNextPopup(res, TextCode::ErrMsg_Header, TextCode::ErrMsg_Footer);
	}

	// ------------------------------------------   S A V E   -------------------------------------------
	void PixelStudioApp::saveClicked()
	{
		if (m_tabs.empty()) return;

		std::string fileName  = m_tabs[m_IDX].dstPath.empty() ? m_tabs[m_IDX].srcPath.filename().string() : m_tabs[m_IDX].dstPath.filename().string();
		std::string parentDir = m_tabs[m_IDX].dstPath.empty() ? DEFAULT_SAVE_PATH.string() : m_tabs[m_IDX].dstPath.parent_path().string();

		fs::path suggestedFileName(fileName);
		if (m_tabs[m_IDX].chanCode == 4)
			suggestedFileName.replace_extension(".png");

		fs::path selectedPath = SysInfo::saveFileDialog(parentDir, suggestedFileName.string());

		if (selectedPath.empty()) return;

		Result res = m_processor.saveImage(selectedPath);

		setNextLog(res);

		if (res.success)
			m_tabs[m_IDX].dstPath = selectedPath;
		else
			setNextPopup(res, TextCode::ErrMsg_Header, TextCode::ErrMsg_Footer);
	}

	// ----------------------------------------   S E L E C T   -----------------------------------------
	void PixelStudioApp::tabSelected(int idx)
	{
		Result res;
		m_IDX = idx;

		m_activeTexID = m_tabs[m_IDX].texID;

		m_processor.selectImage(m_IDX, res);
		setNextLog(res);
	}

	// -----------------------------------------   C L O S E   ------------------------------------------
	void PixelStudioApp::tabClosed(int idx)
	{
		glDeleteTextures(1, &m_tabs[idx].texID);																// clean the texture of the GPUs VRAM

		if (m_inspectionData.stamp == m_tabs[idx].stamp)														// if inspection data belongs to closing TAB
			m_inspectionData.clear();																			// clear inspection data

		updateAvailVRAM(VRAM::DEALLOC, m_tabs[idx].width, m_tabs[idx].height);

		m_tabs.erase(m_tabs.begin() + idx);

		if (idx < m_IDX)	   m_IDX -= 1;																		// _ [X] _  IDX					-= 1
		else if (idx == m_IDX) m_IDX = static_cast<int>(m_tabs.size()) - 1;										// _ _ _ _ [IDX] _  _			to last
																												// _ _ _ _  IDX	 _ [X] _		no Op

		Result res = m_processor.closeImage(idx);

		if (m_IDX != -1)																						// if any TAB still exist
		{
			m_activeTexID = m_tabs[m_IDX].texID;
			res = m_processor.selectImage(m_IDX, res);
		}
		else																									// else all closed
			m_activeTexID = 0;

		setNextLog(res);
	}


	// =======================================================================================================================================================================
	// -------------------------------------------------------------------------  Helper Functions  --------------------------------------------------------------------------
	// =======================================================================================================================================================================

	/**
	 * @brief Loads the data of the passed `ImageTab` as 2D texture into the VRAM of the GPU via GLFW and OpenGL.
	 * @param tab the `ImageTab` to load the texture from
	 * @param buff an `ImageBufferView` which is updated and provided by the `ImageProcesseor` like a snapshot for temporary use
	 */
	void PixelStudioApp::setTextureID(ImageTab & tab, const ImageBufferView& buff)
	{
		// if a texture already exists for this tab, release the old one
		if (tab.texID == 0)
			glGenTextures(1, &tab.texID);																		// request a new texture ID on the GPU

		// bind IN
		glBindTexture(GL_TEXTURE_2D, tab.texID);

		// texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Byte alignment (/wo padding)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		// upload pixel
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, buff.width, buff.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buff.RGBA.data());

		// bind OUT
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	/**
	 * @brief Updates the current TAB's buffer by the passed `ImageBufferView` pointer and also invokes the update of the 2D texture on the VRAM.
	 * @param buffer an `ImageBufferView` which is updated and provided by the `ImageProcesseor` like a snapshot for temporary use
	 */
	void PixelStudioApp::updateBuffer(const ImageBufferView& buff)
	{
		if (!buff.RGBA.empty())
		{
			// update the meta data
			m_tabs[m_IDX].stamp = buff.stamp;
			m_tabs[m_IDX].width = buff.width;
			m_tabs[m_IDX].height = buff.height;
			m_tabs[m_IDX].chanCode = buff.chanCode;

			glBindTexture(GL_TEXTURE_2D, m_tabs[m_IDX].texID);

			// overwrites only the pixel data in the existing VRAM memory area (Zero-Realloc)
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, buff.width, buff.height, GL_RGBA, GL_UNSIGNED_BYTE, buff.RGBA.data());
			glBindTexture(GL_TEXTURE_2D, 0);

			m_activeTexID = m_tabs[m_IDX].texID;
		}
	}

	/**
	 * @brief Organizes the Performance Log widget and sets the next log to show
	 */
	void PixelStudioApp::setNextLog(const Result & res)
	{
		m_currLog.clear();
		m_currLog.reserve(256);

		for (LogEntry const &entry : res.logs)
			m_currLog += formatEntry(entry.code, entry.args);
	}

	/**
	 * @brief Organizes the pending next popup window and its informations to show.
	 * @param res `Result` type including informations. In this case the popup informations like text, header and footer
	 * @param header the header of the build next popup
	 * @param footer the footer of the build next popup
	 */
	void PixelStudioApp::setNextPopup(const Result & res, TextCode header, TextCode footer)
	{
		m_currPopupText.reserve(1024);

		for (LogEntry const &entry : res.logs)
			m_currPopupText += formatEntry(entry.code, entry.args);

		m_header = header;
		m_footer = footer;

		m_popupToShow = true;
	}

	/**
	 * @brief Queries GPU infos by OpenGL. Captures the vendor, renderer and the available VRAM at start for this app of the systems GPU.
	 */
	void PixelStudioApp::queryGPUInfo()
	{
		const GLubyte* vendorStr   = glGetString(GL_VENDOR);
		const GLubyte* rendererStr = glGetString(GL_RENDERER);

		if (vendorStr)	 m_GPU.vendorStr   = reinterpret_cast<const char*>(vendorStr);
		if (rendererStr)
		{
			m_GPU.rendererStr = reinterpret_cast<const char*>(rendererStr);

			size_t bracketPos = m_GPU.rendererStr.find('(');													// extract clean GPU name (cutting off Mesa/Linux additional infos)
			if (bracketPos != std::string::npos)
			{
				m_GPU.rendererStr = m_GPU.rendererStr.substr(0, bracketPos);
				m_GPU.rendererStr.erase(m_GPU.rendererStr.find_last_not_of(" \t") + 1);							// delete trailing spaces
			}
		}

		// NVIDIA: Check initial free VRAM available for this app
		if (m_GPU.vendorStr.find("NVIDIA") != std::string::npos)
		{
			m_GPU.vendor = GPU_Vendor::NVIDIA;
			GLint freeKb = 0;
			glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &freeKb);
			if (freeKb > 0)
				m_GPU.avail_VRAM = m_GPU.total_VRAM = static_cast<float>(freeKb) * BYTES_TO_KB;
		}

		// AMD | ATI: Check initial free VRAM available for this app
		else if (m_GPU.vendorStr.find("AMD") != std::string::npos || m_GPU.vendorStr.find("ATI") != std::string::npos)
		{
			m_GPU.vendor = GPU_Vendor::AMD;
			GLint memInfo[4] = {0};
			glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, memInfo);
			if (memInfo[0] > 0)
				m_GPU.avail_VRAM = m_GPU.total_VRAM = static_cast<float>(memInfo[0]) * BYTES_TO_KB;
		}
		// Intel (OpenGL doesn't support reliable extensions fo Intel)
		else if (m_GPU.vendorStr.find("Intel") != std::string::npos)
		{
			m_GPU.vendor = GPU_Vendor::Intel;
			m_GPU.avail_VRAM = -1.0f;
		}
	}

	/**
	 * @brief Calculates the actual used byte size on the VRAM by images & inspection data of this app
	 * @param w width of the corresponding image
	 * @param h height of the corresponding image
	 * @param access_mode to VRAM calculations {ALLOC, DEALLOC}
	 */
	void PixelStudioApp::updateAvailVRAM(VRAM access_mode, int w, int h)
	{
		// perform manual calculations instead of simply using the OpenGL API
		if (access_mode == VRAM::ALLOC)
		{
			float amount = toMB(w, h);
			m_GPU.used_VRAM  += amount;
			m_GPU.avail_VRAM -= amount;
		}

		else if (access_mode == VRAM::DEALLOC)
		{
			float amount	  = toMB(w, h);
			m_GPU.used_VRAM  -= amount;
			m_GPU.avail_VRAM += amount;
		}

		else
		{
			// getting the current available VRAM via OpenGL API
			if (m_GPU.vendor == GPU_Vendor::NVIDIA)
			{
				GLint freeKb = 0;
				glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &freeKb);
				m_GPU.avail_VRAM = static_cast<float>(freeKb) * BYTES_TO_KB;
			}
			else if (m_GPU.vendor == GPU_Vendor::AMD)
			{
				GLint memInfo[4];
				glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, memInfo);
				m_GPU.avail_VRAM = static_cast<float>(memInfo[0]) * BYTES_TO_KB;								// memInfo[0] returns the free VRAM in kB
			}
		}
	}

	/**
	 * @brief Calculates and returns the corresponding size in Megabytes of the given parameters.
	 * @param w the width of the image
	 * @param h the height of the image
	 * @return the calculated Megabytes as a `float` value
	 */
	constexpr float PixelStudioApp::toMB(const int w, const int h) noexcept
	{
		return static_cast<float>(static_cast<size_t>(w * h) << 2) * BYTES_TO_MB;
	}


	// =======================================================================================================================================================================
	// --------------------------------------------------------------------  InspectionData Definitions  ---------------------------------------------------------------------
	// =======================================================================================================================================================================

	/**
	 * @brief Manages the `InspectionData` to take place by the "Harris Corner Detection" data
	 * @param w width of the corresponding image
	 * @param h height of the corresponding image
	 */
	void PixelStudioApp::InspectionData::setupHarris(int w, int h)
	{
		clear(); 																								// clear any `InspectionData` of the VRAM
		width  = w;
		height = h;
		stamp  = app.m_processor.getHarrisStamp();

		auto pts = app.m_processor.getKeypoints();
		keypoints.assign(pts.begin(), pts.end());
	}

	/**
	 * @brief Manages the `InspectionData` to take place by the "Scale Invariant Feature Transform (SIFT)" data
	 * @param w width of the corresponding image
	 * @param h height of the corresponding image
	 */
	void PixelStudioApp::InspectionData::setupSIFT(int w, int h)
	{
		// comming soon
	}

	/**
	 * @brief Manages the `InspectionData` to take place by the "Speeded Up Robust Feature (SURF)" data
	 * @param w width of the corresponding image
	 * @param h height of the corresponding image
	 */
	void PixelStudioApp::InspectionData::setupSURF(int w, int h)
	{
		// comming soon
	}

	/**
	 * @brief Clear any of the loaded textures of the VRAM
	 */
	void PixelStudioApp::InspectionData::clear()
	{
		int cnt = 0;

		if (tex_Ix != 0) { glDeleteTextures(1, &tex_Ix); tex_Ix = 0; ++cnt; }
		if (tex_Iy != 0) { glDeleteTextures(1, &tex_Iy); tex_Iy = 0; ++cnt; }
		if (texIxx != 0) { glDeleteTextures(1, &texIxx); texIxx = 0; ++cnt; }
		if (texIyy != 0) { glDeleteTextures(1, &texIyy); texIyy = 0; ++cnt; }
		if (texIxy != 0) { glDeleteTextures(1, &texIxy); texIxy = 0; ++cnt; }

		if (cnt > 0)
			app.updateAvailVRAM(VRAM::DEALLOC, cnt * width, height);

		keypoints.clear();
		keypoints.shrink_to_fit();

		width  = 0;
		height = 0;
		stamp  = 0;
	}

	/**
	 * @brief Gets or fetches the corrensopnding Ix (Sobel in X dir) of the underlaying image. Only at getting the first time the texture is upload to the VRAM.
	 * @return GLuint texture ID
	 */
	GLuint PixelStudioApp::InspectionData::getOrFetchIx()
	{
		if (tex_Ix == 0) { tex_Ix = uploadSingleChannel(app.m_processor.get_Ix()); }
		return tex_Ix;
	}

	/**
	 * @brief Gets or fetches the corrensopnding Iy (Sobel in Y dir) of the underlaying image. Only at getting the first time the texture is upload to the VRAM.
	 * @return GLuint texture ID
	 */
	GLuint PixelStudioApp::InspectionData::getOrFetchIy()
	{
		if (tex_Iy == 0) { tex_Iy = uploadSingleChannel(app.m_processor.get_Iy()); }
		return tex_Iy;
	}

	/**
	 * @brief Gets or fetches the corrensopnding Ixx (Ix.Ix * gaussian) of the underlaying image. Only at getting the first time the texture is upload to the VRAM.
	 * @return GLuint texture ID
	 */
	GLuint PixelStudioApp::InspectionData::getOrFetchIxx()
	{
		if (texIxx == 0) { texIxx = uploadSingleChannel(app.m_processor.get_Ixx()); }
		return texIxx;
	}

	/**
	 * @brief Gets or fetches the corrensopnding Iyy (Iy.Iy * gaussian) of the underlaying image. Only at getting the first time the texture is upload to the VRAM.
	 * @return GLuint texture ID
	 */
	GLuint PixelStudioApp::InspectionData::getOrFetchIyy()
	{
		if (texIyy == 0) { texIyy = uploadSingleChannel(app.m_processor.get_Iyy()); }
		return texIyy;
	}

	/**
	 * @brief Gets or fetches the corrensopnding Ixy (Ix.Iy * gaussian) of the underlaying image. Only at getting the first time the texture is upload to the VRAM.
	 * @return GLuint texture ID
	 */
	GLuint PixelStudioApp::InspectionData::getOrFetchIxy()
	{
		if (texIxy == 0) { texIxy = uploadSingleChannel(app.m_processor.get_Ixy()); }
		return texIxy;
	}

	/**
	 * @brief Updates the Harris Stevens R value based keypoints by the passed threshold
	 * @param threshold the minimum threshold that must be exceeded to classify a pixel as a corner
	 */
	void PixelStudioApp::InspectionData::updateThreshold(Result& res, float threshold)
	{
		auto pts = app.m_processor.getKeypoints(res, threshold);
		keypoints.assign(pts.begin(), pts.end());
	}

	/**
	 * @brief Uploads a grayscale RGBA image using R32F by interpolating the Y channel.
	 * @param data just the Y (intensity) channel of the corresponding data to ispect
	 * @return GLuint texture ID
	 */
	GLuint PixelStudioApp::InspectionData::uploadSingleChannel(std::span<const float> data)
	{
		if (data.empty() || width <= 0 || height <= 0) return 0;

		GLuint texID = 0;
		glGenTextures(1, &texID);
		glBindTexture(GL_TEXTURE_2D, texID);

		// standard filtering for sharp pixel inspection
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// set pixel alignment to 1 byte (important for single-channel data)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		// using R32F: Efficient 32-bit float red channel
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, data.data());

		// map the R channel to R, G, and B -> A true grayscale image without wasting VRAM!
		GLint swizzleMask [] = {GL_RED, GL_RED, GL_RED, GL_ONE};
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

		glBindTexture(GL_TEXTURE_2D, 0);

		app.updateAvailVRAM(VRAM::ALLOC, width, height);

		return texID;
	}
}

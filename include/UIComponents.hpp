#pragma once

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <cstdint>


/**
 * @brief
 */
namespace PixelStudio::UI
{
	inline constexpr const char* APP_TITLE = "Pixel Studio";								// app Name
	inline constexpr float MIN_WIDTH  = 1280.0f;											// minimum WIDTH  of the app window (1080p displays)
	inline constexpr float MIN_HEIGHT =  720.0f;											// minimum HEIGHT of the app window (1080p displays)

	inline static int SCALED_W = 0;															// DPI scaled WIDTH
	inline static int SCALED_H = 0;															// DPI scaled HEIGHT

	inline static float em = 15.0f;															// DPI scaled BaseFont size (target: 15.0f at 1080p displays)

	inline static ImFont* BaseFont = nullptr;												// base font of the app		(Roboto-Medium)
	inline static ImFont* MathFont = nullptr;												// font for italic symbols	(Roboto-MediumItalic)
	inline static ImFont* ScutFont = nullptr;												// shortcut font in MenuBar	(Roboto_Condensed-MediumItalic)
	inline static ImFont* InfoFont = nullptr;												// used as monospaced font	(RobotoMono-Medium)

	inline static float Selector_W;															// DPI scaled width for the "Selector Area"
	inline static float OpenLog_H;															// DPI scaled height for the performance logs (when open)
	inline static float InfoBar_H;															// DPI scaled height for the info bar one liner
	inline static float Padding_X3;															// DPI scaled width for 3 times the size of Padding.x
	inline static float Padding_X2;															// DPI scaled width for 2 times the size of Padding.x
	inline static float SliderA_W;															// DPI scaled width for the sliders in Global Manips.
	inline static float SliderB_W;															// DPI scaled width for the sliders in Local Manips.
	inline static float SpcShortcut;														// DPI scaled spacing for the shortcut text (MenuItem)
	inline static float BtnRadius;															// CustomTabButton & Mode Switcher radius
	inline static float Line;																// DPI scaled Line thickness
	inline static float Line_X2;															// DPI scaled 2 Lines

	inline static ImVec2 CheckVec2;															// DPI scaled distance and height for checkboxes
	inline static ImVec2 TabDim;															// DPI scaled dimension for "Custom Tab Buttons"
	inline static ImVec2 TabDimX2;															// DPI scaled 2 TabDim's
	inline static ImVec2 TabDim_Hlf;														// DPI scaled half the size of a TabDim
	inline static ImVec2 Padding;															// DPI scaled dimension of global padding (== style.ItemSpacing)

	inline static ImVec4 VRAM_GOOD;															// app is using VRAM  < 50% (ThemeMode specific)
	inline static ImVec4 VRAM_WARN;															// app is using VRAM  < 80% (ThemeMode specific)
	inline static ImVec4 VRAM_ALERT;														// app is using VRAM >= 80% (ThemeMode specific)

	inline constexpr ImColor CloseRED_X		= ImColor(220, 60, 60, 255);					// TAB close button color
	inline constexpr ImVec4 TransparentVec4 = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);				// Transparent ImVec4


	/**
	 * @brief Enum for the visual theme mode of the app {LYTE, DARK}.
	 */
	enum class ThemeMode : uint8_t { LYTE = 0, DARK = 1 };

	/**
	 * @brief Sets the UI base dimensions scaled by the system specific DPI.
	 * @param yScale system specific vertical DPI scale, since ImGui is Font height bound
	 */
	inline void scaleDimensions()
	{
		Selector_W	= 18.666666f * em;									//  280.0
		SliderA_W	= 14.400000f * em;									//  216.0
		SliderB_W	= 13.600000f * em;									//  204.0
		OpenLog_H	=  5.733333f * em;									//   86.0
		SpcShortcut	=  4.000000f * em;									//   60.0
		Padding_X3	=  1.600000f * em;									//   24.0
		InfoBar_H	=  1.333333f * em;									//   20.0
		Padding_X2	=  1.066666f * em;									//   16.0
		BtnRadius	=  0.333333f * em;									//    5.0
		Line_X2		=  0.133333f * em;									//    2.0
		Line		=  0.066666f * em;									//    1.0

		TabDimX2	= ImVec2(1.200000f * em, 0.666666f * em);			//   18.0, 10.0
		TabDim		= ImVec2(0.600000f * em, 0.333333f * em);			//    9.0,  5.0
		Padding		= ImVec2(0.533333f * em, 0.266666f * em);			//    8.0,  4.0
		TabDim_Hlf	= ImVec2(0.300000f * em, 0.166666f * em);			//    4.5,  2.5
		CheckVec2	= ImVec2(0.133333f * em, 0.033333f * em);			//    2.0,  0.5
	}

	/**
	 * @brief Loads and scales some nice fonts for the GUI ("res/assets/fonts/...")
	 * @param yScale system specific vertical DPI scale, since ImGui is Font height bound
	 */
	inline void setFonts(float yScale)
	{
		ImGuiIO& io = ImGui::GetIO();

		// loading some nice fonts for the UI and scale them by the DPI of the running display
		BaseFont = io.Fonts->AddFontFromFileTTF("res/assets/fonts/Roboto-Medium.ttf", em);
		MathFont = io.Fonts->AddFontFromFileTTF("res/assets/fonts/Roboto-MediumItalic.ttf", em);
		ScutFont = io.Fonts->AddFontFromFileTTF("res/assets/fonts/Roboto_Condensed-MediumItalic.ttf", 14.0f * yScale);
		InfoFont = io.Fonts->AddFontFromFileTTF("res/assets/fonts/RobotoMono-Medium.ttf", em);
	}

	/**
	 * @brief Applies the selected `ThemeMode` colors to the ImGui components.
	 * @param mode enum `ThemeMode` {LYTE, DARK}
	 */
	inline void applyTheme(ThemeMode mode)
	{
		ImGuiStyle& style = ImGui::GetStyle();

		if (mode == ThemeMode::DARK)
		{
			ImGui::StyleColorsDark(&style);													// setting ImGui Dark Theme as base

			VRAM_GOOD  = ImVec4(0.20f, 0.85f, 0.3f, 1.0f);									// VRAM usage = GOOD
			VRAM_WARN  = ImVec4(0.95f, 0.75f, 0.1f, 1.0f);									// VRAM usage = WARN
			VRAM_ALERT = ImVec4(0.95f, 0.15f, 0.1f, 1.0f);									// VRAM usage = ALERT

			ImColor BASE	 = ImColor(46, 52, 60, 255);									// Buttons & Headers BASE
			ImColor HOVERED  = ImColor(62, 72, 88, 255);									// Buttons & Headers HOVER
			ImColor CLICKED  = ImColor(80, 95, 120, 255);									// Buttons & Headers CLICK
			ImColor BORDERS	 = ImColor(42, 46, 52, 255);									// Borders & Separator Lines
			ImColor FRAME_BG = ImColor(14, 18, 22, 255);									// Sliders & Checkboxes Background

			// ================================================ Modifications =================================================

			// Window, Panel & Popup backgrounds (decent anthracit) -----------------------------------------------------------
			style.Colors[ImGuiCol_WindowBg]			  = ImColor(26, 28, 32, 255);			// anthacit (window bg)
			style.Colors[ImGuiCol_ChildBg]			  = ImColor(30, 32, 36, 255);			// slightly brighter for panels
			style.Colors[ImGuiCol_PopupBg]			  = ImColor(34, 36, 40, 255);			// sligthly brighter for popups

			// MenuBar + MenuItems & Headers ----------------------------------------------------------------------------------
			style.Colors[ImGuiCol_MenuBarBg]		  = ImColor(40, 42, 46, 255);			// brighter than window bg
			style.Colors[ImGuiCol_Header]			  = BASE;
			style.Colors[ImGuiCol_HeaderHovered]	  = HOVERED;
			style.Colors[ImGuiCol_HeaderActive]		  = CLICKED;

			// Buttons (decent slate anthracit) -------------------------------------------------------------------------------
			style.Colors[ImGuiCol_Button]			  = BASE;
			style.Colors[ImGuiCol_ButtonHovered]	  = HOVERED;
			style.Colors[ImGuiCol_ButtonActive]		  = CLICKED;

			// Text, Borders & Separators -------------------------------------------------------------------------------------
			style.Colors[ImGuiCol_Text]				  = ImColor(224, 228, 232, 255);		// soft white (no 255 blending)
			style.Colors[ImGuiCol_TextDisabled]		  = ImColor(112, 114, 124, 255);		// inactive text
			style.Colors[ImGuiCol_Border]			  = BORDERS;							// barely visible borders
			style.Colors[ImGuiCol_Separator]		  = BORDERS;							// barely visible separators

			// Slider Frames, Sliders & Checkboxes ----------------------------------------------------------------------------
			style.Colors[ImGuiCol_FrameBg]			  = FRAME_BG;
			style.Colors[ImGuiCol_FrameBgHovered]	  = ImColor(26, 30, 32, 255);
			style.Colors[ImGuiCol_FrameBgActive]	  = ImColor(38, 42, 48, 255);
			style.Colors[ImGuiCol_SliderGrab]		  = HOVERED;
			style.Colors[ImGuiCol_SliderGrabActive]	  = CLICKED;
			style.Colors[ImGuiCol_CheckboxSelectedBg] = FRAME_BG;

			// Popup Window Headers & Buttons ---------------------------------------------------------------------------------
			style.Colors[ImGuiCol_TitleBgActive]	  = HOVERED;
			style.Colors[ImGuiCol_TitleBg]			  = BASE;
			style.Colors[ImGuiCol_TitleBgCollapsed]	  = ImColor(46, 52, 60, 190);			// Button base + transparency
		}

		else // ThemeMode::LYTE
		{
			ImGui::StyleColorsLight(&style);												// setting ImGui Light Theme as base

			VRAM_GOOD  = ImVec4(0.00f, 0.50f, 0.16f, 1.0f);									// VRAM usage = GOOD
			VRAM_WARN  = ImVec4(0.80f, 0.45f, 0.00f, 1.0f);									// VRAM usage = WARN
			VRAM_ALERT = ImVec4(0.80f, 0.10f, 0.10f, 1.0f);									// VRAM usage = ALERT

			ImColor BASE	 = ImColor(172, 204, 244, 255);									// Buttons & Headers BASE
			ImColor HOVERED  = ImColor(102, 170, 250, 255);									// Buttons & Headers HOVER
			ImColor CLICKED  = ImColor(33, 150, 250, 255);									// Buttons & Headers CLICK
			ImColor LINES	 = ImColor(0, 0, 0, 45);										// Borders & Separator Lines
			ImColor FRAME_BG = ImColor(255, 255, 255, 255);									// Sliders & Checkboxes Background

			// ================================================ Modifications =================================================

			// Window, Panel & Popup backgrounds (decent blue) ----------------------------------------------------------------
			style.Colors[ImGuiCol_WindowBg]			  = ImColor(242, 244, 246, 255);
			style.Colors[ImGuiCol_ChildBg]			  = ImColor(0, 0, 0, 0);				// Transparent
			style.Colors[ImGuiCol_PopupBg]			  = ImColor(248, 250, 255, 255);		// slighty darker White

			// MenuBar + MenuItems & Headers ----------------------------------------------------------------------------------
			style.Colors[ImGuiCol_MenuBarBg]		  = ImColor(205, 210, 218, 255);
			style.Colors[ImGuiCol_Header]			  = BASE;
			style.Colors[ImGuiCol_HeaderHovered]	  = HOVERED;
			style.Colors[ImGuiCol_HeaderActive]		  = CLICKED;

			// Buttons (decent slate blue) ------------------------------------------------------------------------------------
			style.Colors[ImGuiCol_Button]			  = BASE;
			style.Colors[ImGuiCol_ButtonHovered]	  = HOVERED;
			style.Colors[ImGuiCol_ButtonActive]		  = CLICKED;

			// Text, Borders & Separators -------------------------------------------------------------------------------------
			style.Colors[ImGuiCol_Text]				  = ImColor(35, 40, 50, 255);			// anthracit like
			style.Colors[ImGuiCol_TextDisabled]		  = ImColor(124, 130, 136, 255);
			style.Colors[ImGuiCol_Border]			  = LINES;
			style.Colors[ImGuiCol_Separator]		  = LINES;

			// Slider Frames & Sliders ----------------------------------------------------------------------------------------
			style.Colors[ImGuiCol_FrameBg]			  = FRAME_BG;
			style.Colors[ImGuiCol_FrameBgHovered]	  = ImColor(246, 248, 250, 255);
			style.Colors[ImGuiCol_FrameBgActive]	  = ImColor(224, 234, 244, 255);
			style.Colors[ImGuiCol_SliderGrab]		  = HOVERED;
			style.Colors[ImGuiCol_SliderGrabActive]	  = ImColor(33, 145, 255, 255);
			style.Colors[ImGuiCol_CheckboxSelectedBg] = FRAME_BG;

			// Popup Window Headers & Buttons ---------------------------------------------------------------------------------
			style.Colors[ImGuiCol_TitleBgActive]	  = HOVERED;
			style.Colors[ImGuiCol_TitleBg]			  = BASE;
			style.Colors[ImGuiCol_TitleBgCollapsed]	  = ImColor(196, 220, 247, 255);
		}
	}

	/**
	 * @brief Conversion Helper: Takes hue (0.0 – 1.0) and returns an ImVec4 / ImU32 for OpenGL/Dear ImGui.
	 * @param hue HSV hue value [0.0, 1.0]
	 * @return `ImGui::ImVec4` as (r,g,b,1.0f) set accoringly to the passed hue
	 */
	inline ImVec4 GetKeypointColor(float hue)
	{
		float r = 0.0f, g = 0.0f, b = 0.0f;
		ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, r, g, b);
		return ImVec4(r, g, b, 1.0f);
	}

	/**
	 * @brief Custom MenuItem /w precisely right aligned shortcut text.
	 * @param label label of the Maneu Item
	 * @param shortcut label of the shortcut
	 * @return `true` when Menu Item is clicked
	 */
	inline bool CustomMenuItem(const char* label, const char* shortcut = nullptr)
	{
		bool clicked = ImGui::MenuItem(label);

		// only the shortcut text gets a different styling
		if (shortcut && shortcut[0] != '\0')
		{
			ImGui::SameLine(SpcShortcut);

			ImGui::PushFont(ScutFont);
			ImGui::TextDisabled("%s", shortcut);
			ImGui::PopFont();
		}

		return clicked;
	}

	/**
	 * @brief Produces a custom TAB item with a very nice look and behavior, and returns `true` when the X is clicked.
	 * @param label the short TAB label
	 * @param fName the full name of the image file
	 * @param isActive `true` when the TAB is the active (focused) one
	 * @param selected out param: `true` when the TAB is clicked
	 * @param mainSectionHovered out param: `true` when the TABs main section is hovered
	 * @return `true` if the closing X is clicked by the user
	 */
	inline bool CustomTabButton(const char* label, const char* fName, bool isActive, bool& selected)
	{
		selected = false;
		bool closeRequested = false;

		ImGuiStyle& style = ImGui::GetStyle();

		// pre calculating the position
		ImVec2 textSize	= ImGui::CalcTextSize(label);
		ImVec2 xSize	= ImGui::CalcTextSize("X");

		ImVec2 pMin = ImGui::GetCursorScreenPos();

		float totalWidth  = textSize.x + xSize.x + UI::TabDimX2.x;
		float totalHeight = (textSize.y > xSize.y ? textSize.y : xSize.y) + UI::TabDim.y;
		ImVec2 pMax		  = ImVec2(pMin.x + totalWidth, pMin.y + totalHeight);

		// hover above the whole TAB button (label + X)
		bool isHovered = ImGui::IsMouseHoveringRect(pMin, pMax);

		// TAB background
		ImGuiCol colorEnum = isActive
			? (isHovered ? ImGuiCol_ButtonHovered : ImGuiCol_ButtonActive)
			: (isHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);

		ImU32 currentBg = ImGui::GetColorU32(colorEnum);

		// draw rounded tab background with theme color
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(pMin, pMax, currentBg, UI::BtnRadius, ImDrawFlags_RoundCornersTop);

		// interactive elements
		ImGui::BeginGroup();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, UI::TabDim_Hlf);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

		// main TAB button
		ImGui::PushStyleColor(ImGuiCol_Button, UI::TransparentVec4);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::TransparentVec4);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::TransparentVec4);

		// static sub-ID "##tab_btn" - becomes unique via the OUTER PushID(tabID)!
		if (ImGui::Button("##tabBtn", ImVec2(textSize.x + UI::TabDim.x, totalHeight)))
			selected = true;

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", fName);													// tooltip: Show the full filename on hover!

		ImGui::PopStyleColor(3);

		// draw the label onto the button using DrawList
		ImVec2 textPos = ImVec2(pMin.x + UI::TabDim_Hlf.x, pMin.y + UI::TabDim_Hlf.y);
		ImU32 textColor = ImGui::GetColorU32(style.Colors[isActive ? ImGuiCol_Text : ImGuiCol_TextDisabled]);
		drawList->AddText(textPos, textColor, label);

		// and the X button
		ImGui::SameLine(0.0f, 0.0f);

		ImVec2 xMin = ImGui::GetCursorScreenPos();
		ImVec2 xMax = ImVec2(xMin.x + xSize.x + UI::TabDim.x, pMax.y);

		bool xIsHovered = ImGui::IsMouseHoveringRect(xMin, xMax);

		if (xIsHovered)
			drawList->AddRectFilled(xMin, xMax, UI::CloseRED_X, UI::BtnRadius, ImDrawFlags_RoundCornersTopRight);

		// the invisible button catching the clicks
		if (ImGui::InvisibleButton("##closeBtn", ImVec2(xMax.x - xMin.x, totalHeight)))
			closeRequested = true;

		// "X" Overlay
		ImU32 xTextColor = ImGui::GetColorU32(
			xIsHovered ? style.Colors[ImGuiCol_Text] : (isActive ? style.Colors[ImGuiCol_Text] : style.Colors[ImGuiCol_TextDisabled])
		);
		ImVec2 xTextPos = ImVec2(xMin.x + UI::TabDim_Hlf.x, xMin.y + UI::TabDim_Hlf.y);
		drawList->AddText(xTextPos, xTextColor, "X");

		ImGui::PopStyleVar(4);
		ImGui::EndGroup();

		return closeRequested;
	}

	/**
	 * @brief Renders the theme toggle button (TTB) at the right edge of the CustomTabBar
	 * @param currTheme current `ThemeMode` {LYTE, DARK}
	 * @param nextTheme out param: receives the new `ThemeMode` upon clicking
	 * @return `true` if the theme has been changed
	 */
	inline bool ThemeToggleButton(UI::ThemeMode currTheme, UI::ThemeMode& nextTheme)
	{
		bool toggled = false;

		const char* label = (currTheme == UI::ThemeMode::DARK) ? "LYTE MODE" : "DARK MODE";							// TTB label (showing the unselected mode)
		const float buttonWidth = ImGui::CalcTextSize(label).x + UI::Padding.x;										// calculate TTB width
		const float rightPosition = ImGui::GetWindowWidth() - buttonWidth - UI::Padding.x;							// calculate TTB position (right end)

		ImGui::SetCursorPosX(rightPosition);																		// shift TTB to right end

		int colorsPushed = 0;

		// slightly reducing the presence in LYTE Mode
		if (currTheme == UI::ThemeMode::LYTE)
		{
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_Button));				// push button color as hover color
			++colorsPushed;
		}

		ImGui::PushStyleColor(ImGuiCol_Button, TransparentVec4);													// push transparent button color
		++colorsPushed;

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, BtnRadius);												// push slightly rounded corners

		if (ImGui::Button(label))																					// draw the TTB
		{
			nextTheme = (currTheme == UI::ThemeMode::DARK) ? UI::ThemeMode::LYTE : UI::ThemeMode::DARK;				// next theme mode if clicked
			toggled = true;
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(colorsPushed);																			// pop the ammount of pushed colors

		return toggled;
	}

	/**
	 * @brief Returns `true` when clicked (the caller handles the toggling).
	 * @param label the label of the toggle button
	 * @param isActive `true` when active
	 * @param size the `ImVec2` size  of the button. Default (0, 0)
	 * @return `true` when clicked
	 */
	inline bool ToggleButton(const char* label, bool isActive, const ImVec2& size = ImVec2(0, 0))
	{
		// when active -> push the colors onto the button
		if (isActive)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
		}

		bool clicked = ImGui::Button(label, size);

		// pop the colors
		if (isActive)
			ImGui::PopStyleColor(3);

		return clicked;
	}
}

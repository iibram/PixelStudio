#pragma once

#include "Types.hpp"

#include <system_error>
#include <version>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <new>

// OS specific
#ifdef _WIN32
#	include <windows.h>
#	include <winnt.h>
#	include <shlobj.h>
//#	include <commdlg.h>
#elif defined(__linux__)
#	include <unistd.h>
#	include <stdio.h>
#endif

// OpenMP specific
#if defined(__has_include) && __has_include(<omp.h>)
#	include <omp.h>
#	if defined(_OPENMP)
#		define PARALLEL_RUN			// omp.h ✅, -fopenmp ✅
#		define VERSION _OPENMP
#	else
#		define OMP_STATUS_CODE 1	// omp.h ✅, -fopenmp ⛔
#	endif
#else
#	define OMP_STATUS_CODE 0		// omp.h ⛔, -fopenmp ⛔
#endif


namespace PixelStudio
{
	/**
	 * @brief A tiny cross-platform systems traverser for detecting the OpenMP status, the physical core number and the underlaying visual style of the running system
	 * (Windows/Linux). Also invokes the systems native file choser for loading / saving files interactivly.
	 * @note Defines the detected stats for the compiler, for compiling just the neccessary lines matching the running systems.
	 */
	namespace SysInfo
	{
		// =============================================================== Default OUT Path Check ====================================================================

		/**
		 * @brief Reads the `config.ini` file and sets the default input & output directories accordingly.
		 * @param res `Result` {succes, logs}. A container within the information pipeline to which messages can be appended at the end
		 * @param iniPath the default `config.ini` file path
		 */
		inline void loadAppConfig(Result& res, const fs::path& iniPath = "res/configs/config.ini")
		{
			if (!fs::exists(iniPath)) return;

			std::ifstream file(iniPath);
			if (!file.is_open()) return;

			// Encapsulatable trim lambda for left/right trim
			auto trim = [](std::string& s) {
				constexpr char whitespace [] = " \t\r\n";
				size_t start = s.find_first_not_of(whitespace);
				if (start == std::string::npos)
				{
					s.clear();
					return;
				}
				size_t end = s.find_last_not_of(whitespace);
				s = s.substr(start, (end - start + 1));
			};

			// parsing starts
			std::string line;
			while (std::getline(file, line))
			{
				// trim whitespace and \r from the beginning and end of the line
				trim(line);

				if (line.empty() || line[0] == ';' || line[0] == '#')
					continue;

				// identify & extract
				bool isLoad = line.starts_with("LoadPath=");
				bool isSave = line.starts_with("SavePath=");

				if (!isLoad && !isSave) continue;

				// extract value string
				std::string pathStr = line.substr(9);
				trim(pathStr);																					// trim any whitespace after '=' or at the end of path

				if (pathStr.empty()) continue;

				fs::path targetPath = fs::path(pathStr).make_preferred();										// enforce the OS format directly during parsing!
				std::error_code ec;

				if (!fs::exists(targetPath))																	// directory doesn't exist? -> create it
					fs::create_directories(targetPath, ec);

				if (fs::is_directory(targetPath, ec))															// directory exist/created? -> set as DEFAULT PATHS
				{
					if (isLoad) DEFAULT_LOAD_PATH = targetPath;
					if (isSave) DEFAULT_SAVE_PATH = targetPath;
				}
			}
		}

		// ================================================================== OpenMP Diagnosis =======================================================================

		/**
		 * @brief Checks the running systems OpenMP status and appends these informations to the given container.
		 * @param res `Result` {succes, logs}. A container within the information pipeline to which messages can be appended at the end
		 */
		inline void performOpenMPDiagnosis(Result& res)
		{
			res.logs.push_back({TextCode::SYS_OMP_Diag_Results, {}});

			#ifdef PARALLEL_RUN			// omp.h ✅, -fopenmp ✅
			{
				res.logs.push_back({TextCode::SYS_OMP_Active, {}});
				res.logs.push_back({TextCode::SYS_OMP_RAW_Version, {static_cast<int>(_OPENMP)}});

				#if VERSION >= 201811
				res.logs.push_back({TextCode::SYS_OMP_V5_Plus, {}});
				#elif VERSION >= 201511
				res.logs.push_back({TextCode::SYS_OMP_V4_5, {}});
				#elif VERSION >= 201307
				res.logs.push_back({TextCode::SYS_OMP_V4_0, {}});
				#else
				res.logs.push_back({TextCode::SYS_OMP_Legacy, {}});
				#endif

				res.logs.push_back({TextCode::SYS_OMP_Available_Threads, {static_cast<int>(omp_get_max_threads())}});
			}

			#elif OMP_STATUS_CODE == 1	// omp.h ✅, -fopenmp ⛔
			{
				#define SERIAL_RUN
				res.logs.push_back({TextCode::SYS_OMP_Inactive, {}});
			}

			#else						// omp.h ⛔, -fopenmp ⛔
			{
				#define SERIAL_RUN
				res.logs.push_back({TextCode::SYS_OMP_Uninstalled, {}});
			}
			#endif
		}

		/**
		 * @brief Traverses the specified OS and tries to get and return the physical core number of the system.
		 * @return the detected (or fallback) optimal number of threads to run OpenMP loops
		 */
		inline uint8_t getPhysicalCoreNum()
		{
			uint8_t logical_cores = std::thread::hardware_concurrency();

			// ======================================== Windows ===========================================
			#ifdef _WIN32
			DWORD length = 0;
			GetLogicalProcessorInformation(nullptr, &length);
			if (length > 0)
			{
				std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
				if (GetLogicalProcessorInformation(&buffer[0], &length))
				{
					uint8_t physical_cores = 0;
					for (const auto &info : buffer)
						if (info.Relationship == RelationProcessorCore)
							++physical_cores;

					return physical_cores;
				}
			}

			// ========================================= Linux ============================================
			#elif defined(__linux__)
			// Reads the number of physical cores from sysfs
			std::ifstream cpuinfo("/sys/devices/system/cpu/cpu0/topology/core_cpus_list"); 						// is the target data available
			if (cpuinfo.good())
			{
				std::ifstream smt("/sys/devices/system/cpu/smt/active"); 										// read if SMT is active or not
				char active;
				if (smt >> active && active == '1') 															// writing smt >> active, and check if it is '1'
					return (logical_cores >> 1);
			}
			#endif

			// ================================= Fallback for both OS =====================================
			return logical_cores > 2 ? (logical_cores >> 1) : 1; // assumtion: system has HT and SMT is active
		}

		/**
		 * @brief Calculates and return the optimal chunk size for the passed data structure size in bytes.
		 * @param struct_size_in_bytes size in bytes of the target data structure
		 * @return the optimal chunk size in bytes as an `uint16_t` type
		 */
		inline uint16_t getChunkSize(uint8_t struct_size_in_bytes)
		{
			uint16_t cache_line_size = 64; 																		// setting the standard cache line size of 64 bytes

			#ifdef __cpp_lib_hardware_interference_size
			cache_line_size = std::hardware_destructive_interference_size; 										// setting the cache line size (if def. in compiling system)
			#endif

			uint16_t elements_per_cache_line = cache_line_size / struct_size_in_bytes;

			// An optimal chunk must be a multiple of a cache line.
			// A very recommended sweetspot is 64 = 2^6 = 1 << 6
			return (elements_per_cache_line << 6);
		}

		// ================================================================== Native File Dialog =====================================================================

		/**
		 * @brief Invokes the native open file dialog of the specific OS and applies various file filters.
		 * @param loadDir the pre selected load directory
		 * @return the user selected path or an empty `std::filesystem` when loading is abborted
		 */
		inline fs::path loadFileDialog(const std::string& loadDir)
		{
			// ======================================== Windows ===========================================
			#if defined(_WIN32)
			char szFile[260] = {0};

			OPENFILENAMEA ofn;
			ZeroMemory(&ofn, sizeof(ofn));

			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = GetActiveWindow();																	// linking with the app window
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.nFilterIndex = 1;
			ofn.lpstrInitialDir = loadDir.c_str();

			ofn.lpstrFilter = "images (*.png;*.jpg;*.bmp)\0*.png;*.jpg;*.bmp\0All Files (*.*)\0*.*\0";
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

			if (GetOpenFileNameA(&ofn) == TRUE)
				return ofn.lpstrFile;

			return ""; 																							// abborted by the user

			// ========================================= Linux ============================================
			#elif defined(__linux__)

			char buffer[1024];
			std::string result = "";

			std::string cmd = "zenity --file-selection --title=\"open image\"";									// construct Zenity command with starting directory

			// pass the initial directory (loadDir) to Zenity
			if (!loadDir.empty())
			{
				fs::path p(loadDir);
				std::string pathStr = p.string();
				if (!pathStr.empty() && pathStr.back() != '/' && pathStr.back() != '\\')
					pathStr += "/";

				cmd += " --filename=\"" + pathStr + "\"";
			}

			cmd += " --file-filter=\"images | *.png *.jpg *.bmp\" --file-filter=\"All Files | *\"";				// define filter formats

			FILE* pipe = popen((cmd + " 2>/dev/null").c_str(), "r");
			if (!pipe) return "";

			if (fgets(buffer, sizeof(buffer), pipe) != NULL)
			{
				result = buffer;

				if (!result.empty() && result.back() == '\n')													// remove the line break (\n) at the end of the path
					result.pop_back();
			}
			pclose(pipe);
			return result;

			// ==================================== Unknown System ========================================
			#else
			return "";
			#endif
		}

		/**
		 * @brief Invokes the native save file dialog of the specific OS and applies a filter for image files
		 * @return the path selection or an empty `std::filesystem` when abborted or unknown OS
		 */
		inline fs::path saveFileDialog(const std::string& saveDir, const std::string& filename)
		{
			// ======================================== Windows ===========================================
			#if defined(_WIN32)

			char szFile[MAX_PATH] = {0};
			strncpy_s(szFile, filename.c_str(), _TRUNCATE);

			OPENFILENAMEA ofn;
			ZeroMemory(&ofn, sizeof(ofn));

			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = GetActiveWindow(); 																	// linking with the app window
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrInitialDir = saveDir.empty() ? NULL : saveDir.c_str();

			// Type filters: Null bytes (\0) separate the name, pattern, and individual filter entries
			ofn.lpstrFilter =
				"All Supported Images (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0"
				"PNG Image (*.png)\0*.png\0"
				"JPEG Image (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
				"Bitmap (*.bmp)\0*.bmp\0"
				"All Files (*.*)\0*.*\0";

			ofn.nFilterIndex = 1; // starts directly with the "All Supported Images" filter
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

			// extract the dynamic default extension based on the filename extension
			fs::path fnPath(filename);
			std::string ext = fnPath.extension().string();
			if (!ext.empty() && ext.front() == '.') ext.erase(0, 1); 											// remove '.' => "png"

			ofn.lpstrDefExt = ext.empty() ? "png" : ext.c_str();

			if (GetSaveFileNameA(&ofn) == TRUE)
				return ofn.lpstrFile;

			return "";																							// abborted by the user

			// ========================================= Linux ============================================
			#elif defined(__linux__)

			fs::path fullInitialPath = fs::path(saveDir) / filename;

			// construct the command string for Zenity
			std::string cmd = "zenity --file-selection --save --confirm-overwrite "
				"--title=\"Save Image As...\" "
				"--filename=\"" + fullInitialPath.string() + "\" "
				"--file-filter=\"All Supported Images | *.png *.jpg *.jpeg *.bmp *.PNG *.JPG *.JPEG *.BMP\" "
				"--file-filter=\"PNG Image | *.png *.PNG\" "
				"--file-filter=\"JPEG Image | *.jpg *.jpeg *.JPG *.JPEG\" "
				"--file-filter=\"Bitmap | *.bmp *.BMP\" "
				"--file-filter=\"All Files | *\" 2>/dev/null";

			FILE* pipe = popen(cmd.c_str(), "r");
			if (!pipe) return "";

			char buffer[1024] = {0};
			std::string result = "";

			if (fgets(buffer, sizeof(buffer), pipe) != NULL)
			{
				result = buffer;

				if (!result.empty() && result.back() == '\n')													// remove the line break (\n) at the end of the path
					result.pop_back();
			}
			pclose(pipe);

			if (result.empty())
				return "";

			// force a dynamic fallback for the file extension (corresponds to lpstrDefExt on Windows)
			fs::path resPath(result);
			if (!resPath.has_extension())
			{
				fs::path fnPath(filename);
				std::string ext = fnPath.extension().string();
				if (ext.empty()) ext = ".png";
				if (ext.front() != '.') ext = "." + ext;

				result += ext;
			}

			return result;

			// ==================================== Unknown System ========================================
			#else
			return "";
			#endif
		}

		// ============================================================ Default DIR Dialog (config.ini) ==============================================================

		/**
		 * @brief
		 * @param title
		 * @return selected directory path
		 */
		inline fs::path selectFolderDialog(const std::string& title = "Select Directory")
		{
			#if defined(_WIN32)
			BROWSEINFOA bi = {0};
			bi.lpszTitle = title.c_str();
			bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

			LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
			if (pidl != nullptr)
			{
				char path[MAX_PATH];
				if (SHGetPathFromIDListA(pidl, path))
				{
					CoTaskMemFree(pidl);
					return fs::path(path).make_preferred();
				}
				CoTaskMemFree(pidl);
			}
			#endif
			return "";
		}

		/**
		 * @brief
		 * @param iniPath
		 */
		inline void saveAppConfig(const fs::path& iniPath = "res/configs/config.ini")
		{
			// ensures that the res/configs directory exists
			if (iniPath.has_parent_path())
			{
				std::error_code ec;
				fs::create_directories(iniPath.parent_path(), ec);
			}

			std::ofstream file(iniPath);
			if (!file.is_open()) return;

			file << "; =========================================================\n";
			file << "; Pixel Studio Configuration File\n";
			file << "; =========================================================\n\n";
			file << "[Paths]\n";
			file << "LoadPath=" << DEFAULT_LOAD_PATH.make_preferred().string() << "\n";
			file << "SavePath=" << DEFAULT_SAVE_PATH.make_preferred().string() << "\n";
		}

		// =================================================================== OS Visual Style =======================================================================

		/**
		 * @brief Detects and returns the visual style of the specific OS.
		 * @return `true` if dark mode is detected else `false`
		 */
		inline void checkVisualMode(Result &res)
		{
			res.success = true;																					// Assumtion: Dark Mode is active

			// ======================================== Windows ===========================================
			#if defined(_WIN32)

			DWORD data = 0;
			DWORD dataSize = sizeof(data);

			// checks Windows setups for apps (0 = Dark, 1 = Light)
			LONG result = RegGetValueA(
				HKEY_CURRENT_USER,
				"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
				"AppsUseLightTheme",
				RRF_RT_REG_DWORD,
				NULL,
				&data,
				&dataSize
			);

			if (result == ERROR_SUCCESS)
			{
				if (data == 0)																					// when AppsUseLightTheme 0 -> Dark Mode is active!
					res.logs.push_back({TextCode::SYS_OS_Visual_Mode, {"Windows in Dark Mode"}});
				else
				{
					res.success = false;
					res.logs.push_back({TextCode::SYS_OS_Visual_Mode, {"Windows in Light Mode"}});
				}

			}
			else
				res.logs.push_back({TextCode::SYS_OS_Unknown_Dark_Mode, {}});									// fallback to Dark Mode

			// ========================================= Linux ============================================
			#elif defined(__linux__)

			char buffer[256];
			std::string result = "";

			// primary method: FreeDesktop XDG Portal query (KDE Plasma 6 / CachyOS, GNOME, Wayland/X11)
			FILE* pipe = popen(
				"dbus-send --session --print-reply=literal --dest=org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop "
				"org.freedesktop.portal.Settings.Read string:'org.freedesktop.appearance' string:'color-scheme' 2>/dev/null", "r"
			);

			if (pipe)
			{
				if (fgets(buffer, sizeof(buffer), pipe) != NULL) { result = buffer; }
				pclose(pipe);

				// XDG Portal Standard: 1 = Prefer Dark, 2 = Prefer Light, 0 = No Preference
				if (result.find("uint32 1") != std::string::npos)
				{
					res.logs.push_back({TextCode::SYS_OS_Visual_Mode, {"Linux in Dark Mode"}});
					return;
				}
				else if (result.find("uint32 2") != std::string::npos)
				{
					res.success = false;
					res.logs.push_back({TextCode::SYS_OS_Visual_Mode, {"Linux in Light Mode"}});
					return;
				}
			}

			// secondary fallback: GNOME / XFCE / Legacy GTK via gsettings
			pipe = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
			if (pipe)
			{
				result = "";
				if (fgets(buffer, sizeof(buffer), pipe) != NULL) { result = buffer; }
				pclose(pipe);

				if (result.find("dark") != std::string::npos)
				{
					res.logs.push_back({TextCode::SYS_OS_Visual_Mode, {"Linux in Dark Mode"}});
					return;
				}
				else if (result.find("light") != std::string::npos || result.find("default") != std::string::npos)
				{
					res.success = false;
					res.logs.push_back({TextCode::SYS_OS_Visual_Mode, {"Linux in Light Mode"}});
					return;
				}
			}

			// ultimate fallback: If both queries fail
			res.logs.push_back({TextCode::SYS_OS_Unknown_Dark_Mode, {}});											// fallback to Dark Mode

			// ==================================== Unknown System ========================================
			#else
			res.logs.push_back({TextCode::SYS_OS_Unknown_Dark_Mode, {}});											// fallback to Dark Mode
			#endif
		}
	}
}

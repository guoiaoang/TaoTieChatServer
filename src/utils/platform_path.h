#ifndef PLATFORM_PATH_H
#define PLATFORM_PATH_H

#include <string>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shlobj.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <unistd.h>
#include <pwd.h>
#endif

namespace PathEx {

namespace detail {

inline std::string wstring_to_string(const std::wstring& wstr) {
#if defined(_WIN32) || defined(_WIN64)
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string str(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr);
    return str;
#else
    return std::string(wstr.begin(), wstr.end());
#endif
}

}

class PlatformPath {
public:
    static std::string getHomeDir() {
#if defined(_WIN32) || defined(_WIN64)
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &path))) {
            std::string result = detail::wstring_to_string(path);
            CoTaskMemFree(path);
            return result;
        }
        // const char* home = getenv("USERPROFILE");
        // if (home && home[0] != '\0') return std::string(home);
        // return "";
        char* buffer = nullptr;
        size_t size = 0;
        // 使用安全版本
        if (_dupenv_s(&buffer, &size, "USERPROFILE") == 0 && buffer != nullptr) {
            std::string result(buffer);
            free(buffer);
            return result;
        }
#elif defined(__linux__) && !defined(__ANDROID__)
        const char* home = getenv("HOME");
        if (home && home[0] != '\0') return std::string(home);
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') return std::string(pw->pw_dir);
        return "";
#elif defined(__ANDROID__)
        const char* android_data = getenv("ANDROID_DATA");
        if (android_data && android_data[0] != '\0') return std::string(android_data);
        return "/data/data";
#elif defined(__APPLE__)
        const char* home = getenv("HOME");
        if (home && home[0] != '\0') return std::string(home);
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') return std::string(pw->pw_dir);
        return "";
#else
        return "";
#endif
        return "";
    }

    static std::string getDocumentsDir() {
#if defined(_WIN32) || defined(_WIN64)
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path))) {
            std::string result = detail::wstring_to_string(path);
            CoTaskMemFree(path);
            return result;
        }
        return "";
#elif defined(__linux__) && !defined(__ANDROID__)
        const char* xdg = getenv("XDG_DOCUMENTS_DIR");
        if (xdg && xdg[0] != '\0') return std::string(xdg);
        std::string home = getHomeDir();
        return home.empty() ? "" : home + "/Documents";
#elif defined(__ANDROID__)
        return "/storage/emulated/0/Documents";
#elif defined(__APPLE__)
        NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        if (paths && [paths count] > 0) {
            NSString* path = [paths objectAtIndex:0];
            const char* cpath = [path UTF8String];
            if (cpath) return std::string(cpath);
        }
        std::string home = getHomeDir();
        return home.empty() ? "" : home + "/Documents";
#else
        return "";
#endif
    }

    static std::string getDownloadsDir() {
#if defined(_WIN32) || defined(_WIN64)
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &path))) {
            std::string result = detail::wstring_to_string(path);
            CoTaskMemFree(path);
            return result;
        }
        return "";
#elif defined(__linux__) && !defined(__ANDROID__)
        const char* xdg = getenv("XDG_DOWNLOAD_DIR");
        if (xdg && xdg[0] != '\0') return std::string(xdg);
        std::string home = getHomeDir();
        return home.empty() ? "" : home + "/Downloads";
#elif defined(__ANDROID__)
        return "/storage/emulated/0/Download";
#elif defined(__APPLE__)
        NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDownloadDirectory, NSUserDomainMask, YES);
        if (paths && [paths count] > 0) {
            NSString* path = [paths objectAtIndex:0];
            const char* cpath = [path UTF8String];
            if (cpath) return std::string(cpath);
        }
        std::string home = getHomeDir();
        return home.empty() ? "" : home + "/Downloads";
#else
        return "";
#endif
    }

    static std::string getPicturesDir() {
#if defined(_WIN32) || defined(_WIN64)
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &path))) {
            std::string result = detail::wstring_to_string(path);
            CoTaskMemFree(path);
            return result;
        }
        return "";
#elif defined(__linux__) && !defined(__ANDROID__)
        const char* xdg = getenv("XDG_PICTURES_DIR");
        if (xdg && xdg[0] != '\0') return std::string(xdg);
        std::string home = getHomeDir();
        return home.empty() ? "" : home + "/Pictures";
#elif defined(__ANDROID__)
        return "/storage/emulated/0/Pictures";
#elif defined(__APPLE__)
        NSArray* paths = NSSearchPathForDirectoriesInDomains(NSPicturesDirectory, NSUserDomainMask, YES);
        if (paths && [paths count] > 0) {
            NSString* path = [paths objectAtIndex:0];
            const char* cpath = [path UTF8String];
            if (cpath) return std::string(cpath);
        }
        std::string home = getHomeDir();
        return home.empty() ? "" : home + "/Pictures";
#else
        return "";
#endif
    }

    static std::string getVideosDir() {
#if defined(_WIN32) || defined(_WIN64)
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, NULL, &path))) {
            std::string result = detail::wstring_to_string(path);
            CoTaskMemFree(path);
            return result;
        }
        return "";
#elif defined(__linux__) && !defined(__ANDROID__)
        const char* xdg = getenv("XDG_VIDEOS_DIR");
        if (xdg && xdg[0] != '\0') return std::string(xdg);
        std::string home = getHomeDir();
        return home.empty() ? "" : home + "/Videos";
#elif defined(__ANDROID__)
        return "/storage/emulated/0/DCIM";
#elif defined(__APPLE__)
        NSArray* paths = NSSearchPathForDirectoriesInDomains(NSMoviesDirectory, NSUserDomainMask, YES);
        if (paths && [paths count] > 0) {
            NSString* path = [paths objectAtIndex:0];
            const char* cpath = [path UTF8String];
            if (cpath) return std::string(cpath);
        }
        std::string home = getHomeDir();
        return home.empty() ? "" : home + "/Movies";
#else
        return "";
#endif
    }

    static std::string getTempDir() {
#if defined(_WIN32) || defined(_WIN64)
        char path[MAX_PATH];
        if (GetTempPathA(MAX_PATH, path)) {
            return std::string(path);
        }
        return "";
#elif defined(__linux__) || defined(__ANDROID__) || defined(__APPLE__)
        const char* temp = getenv("TMPDIR");
        if (temp && temp[0] != '\0') return std::string(temp);
        temp = getenv("TEMP");
        if (temp && temp[0] != '\0') return std::string(temp);
        temp = getenv("TMP");
        if (temp && temp[0] != '\0') return std::string(temp);
        return "/tmp";
#else
        return "";
#endif
    }

    static std::string getExecutableDir() {
#if defined(_WIN32) || defined(_WIN64)
        WCHAR path[MAX_PATH];
        if (GetModuleFileNameW(NULL, path, MAX_PATH)) {
            std::wstring wpath(path);
            size_t pos = wpath.find_last_of(L'\\');
            if (pos != std::wstring::npos) {
                return detail::wstring_to_string(wpath.substr(0, pos));
            }
            return detail::wstring_to_string(wpath);
        }
        return "";
#elif defined(__linux__) || defined(__ANDROID__)
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, PATH_MAX - 1);
        if (len > 0 && len < PATH_MAX) {
            path[len] = '\0';
            std::string strpath(path);
            size_t pos = strpath.find_last_of('/');
            if (pos != std::string::npos) {
                return strpath.substr(0, pos);
            }
            return strpath;
        }
        return "";
#elif defined(__APPLE__)
        char path[PATH_MAX];
        uint32_t size = PATH_MAX;
        if (_NSGetExecutablePath(path, &size) == 0) {
            std::string strpath(path);
            size_t pos = strpath.find_last_of('/');
            if (pos != std::string::npos) {
                return strpath.substr(0, pos);
            }
            return strpath;
        }
        return "";
#else
        return "";
#endif
    }

    static std::string getCurrentDir() {
        try {
            return std::filesystem::current_path().string();
        } catch (...) {
            return "";
        }
    }
};

// #include "platform_path.h"
// #include <iostream>
// int main() {
//     std::cout << "Home: " << PathEx::PlatformPath::getHomeDir() << std::endl;
//     std::cout << "Documents: " << PathEx::PlatformPath::getDocumentsDir() << std::endl;
//     std::cout << "Downloads: " << PathEx::PlatformPath::getDownloadsDir() << std::endl;
//     return 0;
// }
}

#endif
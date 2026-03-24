#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace {

std::wstring ParentDirectory(const std::wstring &path) {
    const auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    return path.substr(0, pos);
}

std::wstring JoinPath(const std::wstring &lhs, const std::wstring &rhs) {
    if (lhs.empty()) return rhs;
    if (rhs.empty()) return lhs;
    if (lhs.back() == L'\\' || lhs.back() == L'/') return lhs + rhs;
    return lhs + L"\\" + rhs;
}

std::wstring ReadEnv(const wchar_t *name) {
    const auto size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) return {};
    std::wstring value(size - 1, L'\0');
    GetEnvironmentVariableW(name, value.data(), size);
    return value;
}

std::wstring QuoteArgument(const std::wstring &arg) {
    if (arg.empty()) return L"\"\"";

    bool needsQuotes = false;
    for (const auto ch: arg) {
        if (ch == L' ' || ch == L'\t' || ch == L'"') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return arg;

    std::wstring quoted = L"\"";
    int backslashes = 0;
    for (const auto ch: arg) {
        if (ch == L'\\') {
            backslashes++;
            continue;
        }
        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring BuildForwardedCommandLine(const std::wstring &programPath) {
    std::wstring commandLine = QuoteArgument(programPath);
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv != nullptr) {
        for (int index = 1; index < argc; ++index) {
            commandLine += L" ";
            commandLine += QuoteArgument(argv[index]);
        }
        LocalFree(argv);
    }
    return commandLine;
}

void ShowLaunchError(const std::wstring &message) {
    MessageBoxW(nullptr, message.c_str(), L"Proxor", MB_OK | MB_ICONERROR);
}

// Silently delete a file or directory tree (no-op if the path does not exist).
void DeletePathSilently(const std::wstring &path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return; // does not exist

    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        // Recurse into directory before removing it.
        const std::wstring pattern = path + L"\\*";
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                DeletePathSilently(path + L"\\" + fd.cFileName);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        RemoveDirectoryW(path.c_str());
    } else {
        SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(path.c_str());
    }
}

// Delete all files in root matching a glob prefix (e.g. L"qtbase").
void DeleteByPrefix(const std::wstring &root, const std::wstring &prefix) {
    const std::wstring pattern = JoinPath(root, prefix + L"*");
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        DeletePathSilently(JoinPath(root, fd.cFileName));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

// Remove directories and files left behind by pre-v1.2.0 installs.
void RemoveLegacyPaths(const std::wstring &root) {
    static const wchar_t *legacy[] = {
        L"platforms", L"styles", L"tls", L"iconengines",
        L"imageformats", L"networkinformation", L"sqldrivers",
        L"translations", L"generic", L"public_res", L"runtime",
        L"proxor.png", L"proxor_gui.exe", L"app.exe",
        L"geoip.dat", L"geoip.db", L"geosite.dat", L"geosite.db",
        nullptr
    };
    for (int i = 0; legacy[i]; ++i) {
        DeletePathSilently(JoinPath(root, legacy[i]));
    }
    DeleteByPrefix(root, L"qtbase");
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t buffer[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        ShowLaunchError(L"Failed to resolve the Proxor launcher path.");
        return 1;
    }

    const std::wstring launcherPath(buffer, length);
    const auto packageRoot = ParentDirectory(launcherPath);
    const auto runtimeDir = JoinPath(JoinPath(packageRoot, L"config"), L"runtime");
    const auto guiPath = JoinPath(runtimeDir, L"app.exe");

    if (GetFileAttributesW(guiPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        ShowLaunchError(L"config\\runtime\\app.exe was not found. Reinstall or redeploy Proxor.");
        return 1;
    }

    RemoveLegacyPaths(packageRoot);

    SetEnvironmentVariableW(L"NKR_FROM_LAUNCHER", L"1");
    SetEnvironmentVariableW(L"PROXOR_PACKAGE_ROOT", packageRoot.c_str());
    SetEnvironmentVariableW(L"PROXOR_RUNTIME_DIR", runtimeDir.c_str());

    auto path = ReadEnv(L"PATH");
    if (!path.empty()) {
        path = runtimeDir + L";" + path;
    } else {
        path = runtimeDir;
    }
    SetEnvironmentVariableW(L"PATH", path.c_str());
    SetCurrentDirectoryW(packageRoot.c_str());

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    auto commandLine = BuildForwardedCommandLine(guiPath);
    if (!CreateProcessW(guiPath.c_str(),
                        commandLine.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        packageRoot.c_str(),
                        &startupInfo,
                        &processInfo)) {
        ShowLaunchError(L"Failed to launch config\\runtime\\app.exe.");
        return 1;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return 0;
}

#endif

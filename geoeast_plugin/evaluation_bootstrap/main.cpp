#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

namespace {

std::wstring modulePath() {
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= static_cast<DWORD>(buffer.size())) {
        return {};
    }
    return std::wstring(buffer.data(), length);
}

std::wstring parentDirectory(const std::wstring& path) {
    const std::wstring::size_type separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring() : path.substr(0, separator);
}

std::wstring windowsErrorMessage(DWORD error) {
    wchar_t* message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(
        flags, nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring result = length > 0 && message ? std::wstring(message, length) : L"未知 Windows 错误";
    if (message) {
        LocalFree(message);
    }
    return result;
}

void showLaunchError(const std::wstring& detail) {
    MessageBoxW(
        nullptr,
        (L"FaultPolyTrace 评测程序无法启动。\n\n" + detail +
         L"\n\n请保持 FaultPolyTrace.exe 与 pyalgo_faultpolytrace 子目录的相对位置不变。").c_str(),
        L"FaultPolyTrace 启动失败",
        MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandTail, int) {
    const std::wstring self = modulePath();
    const std::wstring base = parentDirectory(self);
    if (base.empty()) {
        showLaunchError(L"无法确定顶层启动器所在目录。");
        return 2;
    }

    const std::wstring runtimeDirectory = base + L"\\pyalgo_faultpolytrace";
    const std::wstring launcher = runtimeDirectory + L"\\faultpolytrace_launcher.exe";
    const DWORD attributes = GetFileAttributesW(launcher.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        showLaunchError(L"未找到内部界面程序：\n" + launcher);
        return 3;
    }

    std::wstring commandLine = L"\"" + launcher + L"\"";
    if (commandTail && *commandTail) {
        commandLine += L" ";
        commandLine += commandTail;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL launched = CreateProcessW(
        launcher.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        runtimeDirectory.c_str(),
        &startup,
        &process);
    if (!launched) {
        const DWORD error = GetLastError();
        showLaunchError(L"内部界面程序路径：\n" + launcher +
                        L"\n\nWindows 错误 " + std::to_wstring(error) + L"：" +
                        windowsErrorMessage(error));
        return 4;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}

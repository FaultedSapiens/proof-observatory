#include "process.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{

class WinHandle
{
public:
    explicit WinHandle(HANDLE handle = nullptr) : handle_(handle) {}
    ~WinHandle() { if (handle_ && handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;
    WinHandle(WinHandle&& other) noexcept : handle_(other.release()) {}
    WinHandle& operator=(WinHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (handle_ && handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
            handle_ = other.release();
        }
        return *this;
    }
    HANDLE get() const { return handle_; }
    HANDLE release() { HANDLE value = handle_; handle_ = nullptr; return value; }
private:
    HANDLE handle_;
};

std::wstring widen(const std::string& value)
{
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) throw std::runtime_error("Invalid UTF-8 argument passed to process runner");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::wstring quote_argument(const std::wstring& value)
{
    if (!value.empty() && value.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        return value;
    std::wstring result(1, L'"');
    std::size_t slashes = 0;
    for (const wchar_t c : value)
    {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'"')
        {
            result.append(slashes * 2 + 1, L'\\');
            result.push_back(L'"');
        }
        else
        {
            result.append(slashes, L'\\');
            result.push_back(c);
        }
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

std::wstring command_line(const std::vector<std::wstring>& arguments)
{
    std::wstring line;
    for (const auto& argument : arguments)
    {
        if (!line.empty()) line.push_back(L' ');
        line += quote_argument(argument);
    }
    return line;
}

void append_pipe(HANDLE pipe, std::string& output)
{
    std::array<char, 8192> buffer{};
    DWORD count = 0;
    while (ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) && count > 0)
        output.append(buffer.data(), count);
}

ProcessResult run_process(const std::vector<std::wstring>& arguments,
                          const std::wstring& working_directory,
                          std::chrono::milliseconds timeout)
{
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE out_read_raw = nullptr, out_write_raw = nullptr;
    HANDLE err_read_raw = nullptr, err_write_raw = nullptr;
    if (!CreatePipe(&out_read_raw, &out_write_raw, &security, 0) ||
        !CreatePipe(&err_read_raw, &err_write_raw, &security, 0))
    {
        if (out_read_raw) CloseHandle(out_read_raw);
        if (out_write_raw) CloseHandle(out_write_raw);
        if (err_read_raw) CloseHandle(err_read_raw);
        if (err_write_raw) CloseHandle(err_write_raw);
        throw std::runtime_error("Failed to create subprocess output pipes (Windows error " +
                                 std::to_string(GetLastError()) + ")");
    }
    WinHandle out_read(out_read_raw), out_write(out_write_raw);
    WinHandle err_read(err_read_raw), err_write(err_write_raw);
    if (!SetHandleInformation(out_read.get(), HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(err_read.get(), HANDLE_FLAG_INHERIT, 0))
        throw std::runtime_error("Failed to configure subprocess output pipes");

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = out_write.get();
    startup.hStdError = err_write.get();
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::wstring mutable_line = command_line(arguments);
    const BOOL created = CreateProcessW(nullptr, mutable_line.data(), nullptr, nullptr, TRUE,
                                         CREATE_NO_WINDOW, nullptr,
                                         working_directory.empty() ? nullptr : working_directory.c_str(),
                                         &startup, &process);
    if (!created)
        throw std::runtime_error("Failed to launch subprocess (Windows error " +
                                 std::to_string(GetLastError()));

    WinHandle process_handle(process.hProcess), thread_handle(process.hThread);
    out_write = WinHandle{};
    err_write = WinHandle{};
    ProcessResult result;
    std::thread stdout_reader(append_pipe, out_read.get(), std::ref(result.output));
    std::thread stderr_reader(append_pipe, err_read.get(), std::ref(result.error_output));
    const auto timeout_count = timeout.count();
    const DWORD timeout_ms = timeout_count <= 0 ? INFINITE :
        static_cast<DWORD>(std::min<long long>(timeout_count, MAXDWORD - 1));
    DWORD wait_result = WaitForSingleObject(process_handle.get(), timeout_ms);
    if (wait_result == WAIT_TIMEOUT)
    {
        result.timed_out = true;
        TerminateProcess(process_handle.get(), ERROR_TIMEOUT);
        wait_result = WaitForSingleObject(process_handle.get(), INFINITE);
    }
    if (wait_result != WAIT_OBJECT_0)
    {
        TerminateProcess(process_handle.get(), 1);
        stdout_reader.join();
        stderr_reader.join();
        throw std::runtime_error("Failed waiting for subprocess completion");
    }
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process_handle.get(), &exit_code))
    {
        stdout_reader.join();
        stderr_reader.join();
        throw std::runtime_error("Failed to retrieve subprocess exit code");
    }
    stdout_reader.join();
    stderr_reader.join();
    result.exit_code = static_cast<int>(exit_code);
    return result;
}

bool wsl_unc_to_linux(const std::string& path, std::string& distro, std::string& linux_path)
{
    std::string_view remainder;
    if (path.rfind("\\\\wsl.localhost\\", 0) == 0)
        remainder = std::string_view(path).substr(16);
    else if (path.rfind("\\\\wsl$\\", 0) == 0)
        remainder = std::string_view(path).substr(7);
    else
        return false;

    const auto separator = remainder.find('\\');
    if (separator == std::string_view::npos)
        throw std::runtime_error("WSL UNC path must include a distribution and Linux path");
    distro.assign(remainder.substr(0, separator));
    linux_path.assign(remainder.substr(separator));
    for (char& c : linux_path) if (c == '\\') c = '/';
    return true;
}

} // namespace

ProcessResult run_command(const std::string& command, const std::string& working_directory,
                          std::chrono::milliseconds timeout)
{
    std::string distro, linux_path;
    std::string normalized_directory = working_directory;
    if (normalized_directory.rfind("\\wsl.localhost\\", 0) == 0 ||
        normalized_directory.rfind("\\wsl$\\", 0) == 0)
        normalized_directory.insert(normalized_directory.begin(), '\\');
    if (wsl_unc_to_linux(normalized_directory, distro, linux_path))
    {
        return run_process({L"wsl.exe", L"--distribution", widen(distro), L"--cd",
                            widen(linux_path), L"--exec", L"/bin/bash", L"-lc", widen(command)}, {}, timeout);
    }
    return run_process({L"cmd.exe", L"/D", L"/S", L"/C", widen(command)}, widen(working_directory), timeout);
}

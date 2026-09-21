#include "process.hpp"

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace
{

std::string quote_windows_argument(
    const std::string& value)
{
    /*
        Basic Windows command-line quoting.

        This is sufficient for the paths and commands
        currently used by Proof Observatory.
    */

    std::string result = "\"";

    for (char c : value)
    {
        if (c == '"')
            result += "\\\"";
        else
            result += c;
    }

    result += "\"";

    return result;
}


bool is_unc_path(
    const std::string& path)
{
    return
        path.rfind("\\\\", 0) == 0;
}


ProcessResult run_command_windows(
    const std::string& command,
    const std::string& working_directory)
{
    const fs::path old_directory =
        fs::current_path();

    try
    {
        fs::current_path(
            working_directory
        );
    }
    catch (const fs::filesystem_error&)
    {
        /*
            UNC paths cannot be used as the current
            directory by cmd.exe.

            Caller handles this separately.
        */
        throw;
    }

    const std::string full_command =
        command + " 2>&1";

    FILE* pipe =
        _popen(
            full_command.c_str(),
            "r"
        );

    if (!pipe)
    {
        fs::current_path(old_directory);

        throw std::runtime_error(
            "Failed to start process: " +
            command
        );
    }

    std::ostringstream output;

    char buffer[4096];

    while (
        fgets(
            buffer,
            sizeof(buffer),
            pipe))
    {
        output << buffer;
    }

    const int exit_code =
        _pclose(pipe);

    fs::current_path(old_directory);

    return {
        exit_code,
        output.str()
    };
}


ProcessResult run_command_wsl(
    const std::string& command,
    const std::string& windows_directory)
{
    /*
        Convert:
            \\wsl.localhost\Ubuntu\home\yash\projects\...

        into:
            /home/yash/projects/...

        and execute:
            wsl.exe -d Ubuntu -- bash -lc "cd ... && command"
    */

    const std::string prefix =
        "\\\\wsl.localhost\\";

    if (
        windows_directory.rfind(
            prefix,
            0
        ) != 0)
    {
        throw std::runtime_error(
            "Unsupported WSL path: " +
            windows_directory
        );
    }

    const std::size_t distro_start =
        prefix.size();

    const std::size_t separator =
        windows_directory.find(
            '\\',
            distro_start
        );

    if (
        separator ==
        std::string::npos)
    {
        throw std::runtime_error(
            "Invalid WSL UNC path: " +
            windows_directory
        );
    }

    const std::string distro =
        windows_directory.substr(
            distro_start,
            separator - distro_start
        );

    std::string linux_path =
        windows_directory.substr(
            separator
        );

    for (char& c : linux_path)
    {
        if (c == '\\')
            c = '/';
    }

    /*
        Use bash -lc so shell built-ins and normal
        command syntax work exactly as expected.
    */

    const std::string shell =
        "cd " +
        quote_windows_argument(
            linux_path
        ) +
        " && " +
        command;

    const std::string full_command =
        "wsl.exe -d " +
        quote_windows_argument(
            distro
        ) +
        " -- bash -lc " +
        quote_windows_argument(
            shell
        ) +
        " 2>&1";

    FILE* pipe =
        _popen(
            full_command.c_str(),
            "r"
        );

    if (!pipe)
    {
        throw std::runtime_error(
            "Failed to start WSL process"
        );
    }

    std::ostringstream output;

    char buffer[4096];

    while (
        fgets(
            buffer,
            sizeof(buffer),
            pipe))
    {
        output << buffer;
    }

    const int exit_code =
        _pclose(pipe);

    return {
        exit_code,
        output.str()
    };
}

} // namespace


ProcessResult run_command(
    const std::string& command,
    const std::string& working_directory)
{
    if (
        is_unc_path(
            working_directory))
    {
        return run_command_wsl(
            command,
            working_directory
        );
    }

    return run_command_windows(
        command,
        working_directory
    );
}
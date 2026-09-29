#pragma once

#include <chrono>
#include <string>

struct ProcessResult
{
    int exit_code = -1;
    bool timed_out = false;
    std::string output;
    std::string error_output;
};

ProcessResult run_command(
    const std::string& command,
    const std::string& working_directory,
    std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()
);

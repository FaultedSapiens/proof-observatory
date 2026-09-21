#pragma once

#include <string>

struct ProcessResult
{
    int exit_code;
    std::string output;
};

ProcessResult run_command(
    const std::string& command,
    const std::string& working_directory
);
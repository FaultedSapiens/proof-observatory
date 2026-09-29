#include "process.hpp"

#include <iostream>
#include <chrono>
#include <string>

int main()
{
    const auto result = run_command(
        "echo stdout-marker & echo stderr-marker 1>&2 & exit /b 7",
        "C:\\Windows\\Temp");

    if (result.exit_code != 7)
    {
        std::cerr << "expected exit code 7, got " << result.exit_code << '\n';
        return 1;
    }
    if (result.output.find("stdout-marker") == std::string::npos)
    {
        std::cerr << "stdout was not captured\n";
        return 2;
    }
    if (result.error_output.find("stderr-marker") == std::string::npos)
    {
        std::cerr << "stderr was not captured\n";
        return 3;
    }
    const auto timed = run_command(
        "for /L %i in (1,1,1000000) do @rem",
        "C:\\Windows\\Temp",
        std::chrono::milliseconds(100));
    if (!timed.timed_out)
    {
        std::cerr << "subprocess timeout was not enforced (exit " << timed.exit_code
                  << ", stderr: " << timed.error_output << ")\n";
        return 4;
    }
    return 0;
}

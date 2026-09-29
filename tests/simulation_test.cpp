#include "simulation.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    namespace fs = std::filesystem;
    const fs::path output = fs::temp_directory_path() / "proof_observatory_simulation_test";
    const fs::path repeat = fs::temp_directory_path() / "proof_observatory_simulation_repeat_test";
    fs::remove_all(output);
    fs::remove_all(repeat);
    SimulationConfig config;
    config.nx = 16;
    config.ny = 16;
    config.steps = 3;
    config.output_every = 2;
    config.poisson_iterations = 200;
    if (run_simulation(config, output) != 0) return 1;
    for (const auto& name : {"run.json", "diagnostics.csv", "field_3.vtk", "vorticity_3.svg"})
    {
        const auto file = output / name;
        if (!fs::exists(file) || fs::file_size(file) == 0)
        {
            std::cerr << "missing simulation output: " << name << '\n';
            return 2;
        }
    }
    std::ifstream csv(output / "diagnostics.csv");
    std::string content((std::istreambuf_iterator<char>(csv)), {});
    csv.close();
    if (content.find("max_abs_vorticity") == std::string::npos ||
        static_cast<std::size_t>(std::count(content.begin(), content.end(), '\n')) < 4)
    {
        std::cerr << "diagnostics output is invalid\n";
        return 3;
    }
    if (run_simulation(config, repeat) != 0) return 4;
    for (const auto& name : {"run.json", "diagnostics.csv", "field_3.vtk", "vorticity_3.svg"})
    {
        std::ifstream first(output / name, std::ios::binary), second(repeat / name, std::ios::binary);
        const std::string first_bytes((std::istreambuf_iterator<char>(first)), {});
        const std::string second_bytes((std::istreambuf_iterator<char>(second)), {});
        if (first_bytes != second_bytes)
        {
            std::cerr << "simulation output is not deterministic: " << name << '\n';
            return 5;
        }
    }
    fs::remove_all(output);
    fs::remove_all(repeat);
    return 0;
}

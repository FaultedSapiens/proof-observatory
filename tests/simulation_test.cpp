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
    const fs::path split = fs::temp_directory_path() / "proof_observatory_simulation_split_test";
    fs::remove_all(output);
    fs::remove_all(repeat);
    fs::remove_all(split);
    SimulationConfig config;
    config.nx = 16;
    config.ny = 16;
    config.steps = 6;
    config.output_every = 2;
    config.checkpoint_every = 2;
    config.poisson_iterations = 200;
    if (run_simulation(config, output) != 0) return 1;
    for (const auto& name : {"run.json", "diagnostics.csv", "field_6.vtk", "vorticity_6.svg", "checkpoint.json"})
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
        static_cast<std::size_t>(std::count(content.begin(), content.end(), '\n')) < 5)
    {
        std::cerr << "diagnostics output is invalid\n";
        return 3;
    }
    if (run_simulation(config, repeat) != 0) return 4;
    for (const auto& name : {"diagnostics.csv", "field_6.vtk", "vorticity_6.svg", "checkpoint.json"})
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

    SimulationConfig partial = config;
    partial.stop_after_step = 4;
    if (run_simulation(partial, split) != 0) return 6;
    SimulationConfig resumed = config;
    resumed.restart_from = split / "checkpoint.json";
    if (run_simulation(resumed, split) != 0) return 7;
    for (const auto& name : {"diagnostics.csv", "field_6.vtk", "vorticity_6.svg", "checkpoint.json"})
    {
        std::ifstream continuous(output / name, std::ios::binary), restarted(split / name, std::ios::binary);
        const std::string a((std::istreambuf_iterator<char>(continuous)), {});
        const std::string b((std::istreambuf_iterator<char>(restarted)), {});
        if (a != b)
        {
            std::cerr << "checkpoint/restart differs from uninterrupted run: " << name << '\n';
            return 8;
        }
    }
    bool rejected_mismatch = false;
    try
    {
        SimulationConfig mismatch = config;
        mismatch.viscosity *= 2;
        mismatch.restart_from = split / "checkpoint.json";
        run_simulation(mismatch, split);
    }
    catch (const std::exception&) { rejected_mismatch = true; }
    if (!rejected_mismatch)
    {
        std::cerr << "restart accepted a checkpoint with a different physics configuration\n";
        return 9;
    }

    fs::remove_all(output);
    fs::remove_all(repeat);
    fs::remove_all(split);
    return 0;
}

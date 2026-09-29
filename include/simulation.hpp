#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

struct SimulationConfig
{
    std::size_t nx = 64;
    std::size_t ny = 64;
    double length_x = 2.0 * 3.14159265358979323846;
    double length_y = 2.0 * 3.14159265358979323846;
    double dt = 0.001;
    std::size_t steps = 1000;
    double viscosity = 0.01;
    double forcing_amplitude = 0.0;
    std::size_t output_every = 100;
    std::size_t poisson_iterations = 1200;
    double poisson_tolerance = 1e-8;
    std::string initial_condition = "gaussian";
    std::string advection_scheme = "upwind";
    std::string time_scheme = "rk2";
    std::string boundary = "periodic";
};

int run_simulation(const SimulationConfig& config, const std::filesystem::path& output_dir);

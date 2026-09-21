#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct MainResult
{
    std::string description;
    std::string declaration;
    std::filesystem::path file;
    int sorry_count = 0;
    std::vector<std::string> axioms;
    std::string comparator_config;
};

struct Alignment
{
    std::string source;
    std::string lean;
    std::string module;
    std::string status;
};

struct Formalization
{
    std::string version;
    std::string project_name;
    std::string description;
    std::vector<std::string> authors;
    std::string license;

    std::string scope;
    int sorry_count = 0;
    int sorry_in_definitions = 0;

    std::vector<MainResult> main_results;
    std::vector<Alignment> alignments;
};

Formalization parse_formalization(
    const std::filesystem::path& file
);
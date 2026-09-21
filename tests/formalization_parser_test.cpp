#include "formalization.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main()
{
    const fs::path temp_dir = fs::temp_directory_path() / "proof_observatory_yaml_test";
    fs::create_directories(temp_dir);

    const fs::path yaml_path = temp_dir / "formalization.yaml";

    const std::string yaml_text = R"YAML(
version: "v0.4"
project:
  name: "NavierStokesAndEuler"
  description: "Test project"
  authors:
    - "OpenAI"
  license: "Apache-2.0"
status:
  scope: "Full formalization of main results."
  sorry_count: 0
  sorry_in_definitions: 0
  main_results:
    - description: "Result A"
      declaration: "NavierStokes.Comparator.navier_stokes_breakdown_R3"
      file: "NavierStokes/ComparatorSolution.lean"
      sorry_count: 0
      axioms:
        - "propext"
      comparator_config: "ComparatorChallenges/NavierStokes.json"
review:
  status: "self-assessed"
alignment:
  namespaces:
    - "NavierStokes.Comparator"
  statements:
    - source: "Theorem 1.1 (Navier–Stokes on ℝ³)"
      lean: "NavierStokes.Comparator.navier_stokes_breakdown_R3"
      module: "NavierStokes.ComparatorSolution"
      status: "proved"
)YAML";

    std::ofstream out(yaml_path);
    out << yaml_text;
    out.close();

    const auto formalization = parse_formalization(yaml_path);

    if (formalization.version != "v0.4")
    {
        std::cerr << "version mismatch\n";
        return 1;
    }

    if (formalization.project_name != "NavierStokesAndEuler")
    {
        std::cerr << "project mismatch\n";
        return 1;
    }

    if (formalization.main_results.size() != 1)
    {
        std::cerr << "main_results mismatch\n";
        return 1;
    }

    if (formalization.alignments.size() != 1)
    {
        std::cerr << "alignment mismatch\n";
        return 1;
    }

    if (formalization.alignments[0].lean != "NavierStokes.Comparator.navier_stokes_breakdown_R3")
    {
        std::cerr << "alignment declaration mismatch\n";
        return 1;
    }

    fs::remove_all(temp_dir);
    return 0;
}

#include "comparator_environment.hpp"

#include <cassert>
#include <string>

int main()
{
    const std::string setup(comparator_tool_environment_setup);
    assert(setup.find("${COMPARATOR_LANDRUN:-}") != std::string::npos);
    assert(setup.find("${COMPARATOR_LEAN4EXPORT:-}") != std::string::npos);
    assert(setup.find("command -v") != std::string::npos);
    assert(setup.find("dirname \"$COMPARATOR_LANDRUN\"") != std::string::npos);
    assert(setup.find("dirname \"$COMPARATOR_LEAN4EXPORT\"") != std::string::npos);
    assert(setup.find("export PATH") != std::string::npos);
    assert(setup.find("export COMPARATOR_LANDRUN COMPARATOR_LEAN4EXPORT") != std::string::npos);
    assert(setup.find("/home/") == std::string::npos);
    assert(setup.find("C:\\") == std::string::npos);

    assert(classify_comparator_failure("", "") == "none");
    assert(classify_comparator_failure("", "COMPARATOR_ENVIRONMENT_ERROR: landrun missing") == "tool_environment_error");
    assert(classify_comparator_failure("", "could not execute external process 'landrun'") == "tool_environment_error");
    assert(classify_comparator_failure("", "unknown module prefix 'ComparatorChallenges'") == "tool_environment_error");
    assert(classify_comparator_failure("Some required targets logged failures", "error: build failed") == "project_build_error");
    assert(classify_comparator_failure("", "Illegal axiom detected") == "comparator_rejection");
    assert(classify_comparator_failure("Exporting #[theorem]", "unexpected exporter crash") == "export_error");
    return 0;
}

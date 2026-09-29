#include "verification.hpp"

#include <cassert>
#include <string>

int main()
{
    assert(std::string(verification_class_for_mode("metadata")) == "metadata_provenance");
    assert(std::string(verification_class_for_mode("structural")) == "structural_source_analysis");
    assert(std::string(verification_class_for_mode("file")) == "lean_kernel_compilation");
    assert(std::string(verification_class_for_mode("module")) == "lean_kernel_compilation");
    assert(std::string(verification_class_for_mode("full")) == "lean_kernel_compilation");
    assert(std::string(verification_class_for_mode("comparator")) == "comparator_independent_verification");
    assert(std::string(verification_class_for_mode("comparator-preflight")) == "comparator_environment_preflight");

    assert(std::string(comparator_status_for_exit(0)) == "verified");
    assert(std::string(comparator_status_for_exit(1)) == "failed");
    return 0;
}

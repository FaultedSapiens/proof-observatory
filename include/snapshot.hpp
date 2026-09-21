#pragma once

#include "formalization.hpp"
#include "inspector.hpp"
#include "lean_indexer.hpp"

#include <filesystem>
#include <string>

void write_snapshot_json(
    const std::filesystem::path& output,
    const ArtifactInfo& artifact,
    const Formalization& formalization,
    const LeanIndex& index
);
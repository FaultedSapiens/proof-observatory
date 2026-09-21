#include "formalization.hpp"

#include <yaml-cpp/yaml.h>

Formalization parse_formalization(
    const std::filesystem::path& file)
{
    YAML::Node root = YAML::LoadFile(file.string());

    Formalization f;

    if (root["version"])
        f.version = root["version"].as<std::string>();

    if (root["project"])
    {
        auto project = root["project"];

        if (project["name"])
            f.project_name =
                project["name"].as<std::string>();

        if (project["description"])
            f.description =
                project["description"].as<std::string>();

        if (project["license"])
            f.license =
                project["license"].as<std::string>();

        if (project["authors"])
        {
            for (const auto& author : project["authors"])
                f.authors.push_back(
                    author.as<std::string>()
                );
        }
    }

    if (root["status"])
    {
        auto status = root["status"];

        if (status["scope"])
            f.scope =
                status["scope"].as<std::string>();

        if (status["sorry_count"])
            f.sorry_count =
                status["sorry_count"].as<int>();

        if (status["sorry_in_definitions"])
            f.sorry_in_definitions =
                status["sorry_in_definitions"].as<int>();

        if (status["main_results"])
        {
            for (const auto& node :
                 status["main_results"])
            {
                MainResult result;

                if (node["description"])
                    result.description =
                        node["description"].as<std::string>();

                if (node["declaration"])
                    result.declaration =
                        node["declaration"].as<std::string>();

                if (node["file"])
                    result.file =
                        node["file"].as<std::string>();

                if (node["sorry_count"])
                    result.sorry_count =
                        node["sorry_count"].as<int>();

                if (node["comparator_config"])
                    result.comparator_config =
                        node["comparator_config"].as<std::string>();

                if (node["axioms"])
                {
                    for (const auto& axiom : node["axioms"])
                        result.axioms.push_back(
                            axiom.as<std::string>()
                        );
                }

                f.main_results.push_back(
                    std::move(result)
                );
            }
        }
    }

    const YAML::Node review_alignment =
        root["review"] && root["review"]["alignment"]
            ? root["review"]["alignment"]
            : YAML::Node();

    const YAML::Node top_level_alignment =
        root["alignment"]
            ? root["alignment"]
            : YAML::Node();

    const YAML::Node alignment_nodes =
        !top_level_alignment.IsNull()
            ? (top_level_alignment["statements"]
                   ? top_level_alignment["statements"]
                   : top_level_alignment)
            : review_alignment;

    if (!alignment_nodes.IsNull())
    {
        if (alignment_nodes.IsSequence())
        {
            for (const auto& node : alignment_nodes)
            {
                Alignment alignment;

                if (node["source"])
                    alignment.source =
                        node["source"].as<std::string>();

                if (node["lean"])
                    alignment.lean =
                        node["lean"].as<std::string>();

                if (node["module"])
                    alignment.module =
                        node["module"].as<std::string>();

                if (node["status"])
                    alignment.status =
                        node["status"].as<std::string>();

                f.alignments.push_back(
                    std::move(alignment)
                );
            }
        }
    }

    return f;
}
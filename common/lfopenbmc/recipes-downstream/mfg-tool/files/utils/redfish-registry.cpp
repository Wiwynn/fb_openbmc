#include "utils/redfish-registry.hpp"

#include "config.hpp"
#include "utils/json.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <iostream>

namespace mfgtool::utils::redfish_registry
{

auto load() -> std::unordered_map<message_id_t, event_t>
{
    PHOSPHOR_LOG2_USING;
    info("Loading registries from {PATH}", "PATH", registry_directory);

    std::unordered_map<message_id_t, event_t> result;

    for (auto& file : std::filesystem::recursive_directory_iterator(
             std::filesystem::path(registry_directory)))
    {
        if (!file.is_regular_file())
        {
            continue;
        }
        if (!file.path().native().ends_with(".json"))
        {
            continue;
        }

        // Load the file and parse the JSON.
        std::ifstream f(file.path());
        auto j = js::parse(f);

        std::string prefix = j["RegistryPrefix"];

        // Helper function to pick out the "OpenBMC_Mapping" field from the OEM
        // section of a JSON blob.
        auto get_openbmc_mapping = [](const auto& j) {
            if (j.contains("Oem") && j["Oem"].contains("OpenBMC_Mapping"))
            {
                return j["Oem"]["OpenBMC_Mapping"];
            }
            return json::empty_map();
        };

        // Iterate through all of the Message's defined in this registry.
        for (const auto& m : j["Message"].items())
        {
            std::string message = m.key();
            const auto& value = m.value();

            auto mapping = get_openbmc_mapping(value);
            if (mapping.empty())
            {
                continue;
            }

            event_t e = {prefix + '.' + message, {}};
            for (const auto& arg : mapping["Args"])
            {
                e.args.push_back(arg["Name"]);
            }
            result.emplace(mapping["Event"], std::move(e));
        }

        // Iterate through all of the OEM.OpenBMC_Mapping sections for the
        // OpenBMC to Redfish standard message mappings.
        auto mapping = get_openbmc_mapping(j);
        for (const auto& m : mapping.items())
        {
            std::string id = m.key();
            const auto& value = m.value();

            event_t e = {value["RedfishEvent"], {}};
            for (const auto& arg : value["Args"])
            {
                e.args.push_back(arg["Name"]);
            }

            result.emplace(id, std::move(e));
        }
    }

    return result;
}

} // namespace mfgtool::utils::redfish_registry

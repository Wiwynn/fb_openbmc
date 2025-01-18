#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace mfgtool::utils::redfish_registry
{

using message_id_t = std::string;
struct event_t
{
    message_id_t id;
    std::vector<std::string> args;
};

/** Load the Redfish Registries and get the map from OpenBMC Message ID to
 *  Redfish Message ID (and args). */
auto load() -> std::unordered_map<message_id_t, event_t>;

} // namespace mfgtool::utils::redfish_registry

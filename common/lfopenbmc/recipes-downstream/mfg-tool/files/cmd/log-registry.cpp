#include "utils/json.hpp"
#include "utils/redfish-registry.hpp"
#include "utils/register.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>
#include <vector>

namespace mfgtool::cmds::log_registry
{
PHOSPHOR_LOG2_USING;

struct command
{
    void init(CLI::App& app)
    {
        auto cmd = app.add_subcommand("log-registry",
                                      "List OpenBMC events from the registry");
        cmd->add_option("prefix", arg_event_prefix,
                        "Redfish Message Id prefix for filtering");
        init_callback(cmd, *this);
    }

    void run()
    {
        std::vector<std::string> result{};

        auto events = utils::redfish_registry::load();

        for (const auto& [id, event] : events)
        {
            if (event.id.starts_with(arg_event_prefix))
            {
                result.push_back(id);
            }
        }

        json::display(js(result));
    }

    std::string arg_event_prefix = "";
};
MFGTOOL_REGISTER(command);

} // namespace mfgtool::cmds::log_registry

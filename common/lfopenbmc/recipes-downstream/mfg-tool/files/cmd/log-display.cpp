#include "utils/dbus.hpp"
#include "utils/json.hpp"
#include "utils/mapper.hpp"
#include "utils/redfish-registry.hpp"
#include "utils/register.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/message.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>

namespace mfgtool::cmds::log_display
{
PHOSPHOR_LOG2_USING;

struct command
{
    void init(CLI::App& app)
    {
        auto cmd = app.add_subcommand("log-display", "Display pending logs.");
        cmd->add_flag("-u,--unresolved-only", arg_unresolved_only,
                      "Only show unresolved logs.");

        init_callback(cmd, *this);
    }

    auto run(sdbusplus::async::context& ctx) -> sdbusplus::async::task<>
    {
        namespace log_entry = dbuspath::log_entry;

        const auto event_defs = utils::redfish_registry::load();

        auto result = json::empty_map();

        info("Finding log entries.");
        co_await utils::mapper::subtree_for_each(
            ctx, log_entry::ns_path, log_entry::interface,

            [&](const auto& path,
                const auto& service) -> sdbusplus::async::task<> {
                if (service != log_entry::service)
                {
                    warning("Entry ({PATH}) not hosted by logging service.",
                            "PATH", path);
                    co_return;
                }

                try
                {
                    auto entry = log_entry::Proxy(ctx)
                                     .service(log_entry::service)
                                     .path(path.str);

                    auto resolved = co_await entry.resolved();
                    if (resolved && arg_unresolved_only)
                    {
                        info("Resolved and filtered out.");
                        co_return;
                    }

                    auto& entry_json =
                        result[std::to_string(co_await entry.id())];

                    auto message = co_await entry.message();
                    entry_json["message"] = message;
                    entry_json["severity"] =
                        sdbusplus::message::convert_to_string(
                            co_await entry.severity());
                    entry_json["event_id"] = co_await entry.event_id();

                    auto additional_data = co_await entry.additional_data();
                    entry_json["additional_data"] = additional_data;
                    entry_json["resolution"] = co_await entry.resolution();
                    entry_json["resolved"] = resolved;

                    auto epoch_to_iso8601 = [](auto ts) {
                        using namespace std::chrono;
                        return std::format(
                            "{:%FT%TZ}",
                            time_point<system_clock>(milliseconds(ts)));
                    };

                    entry_json["timestamp"] =
                        epoch_to_iso8601(co_await entry.timestamp());
                    entry_json["updated_timestamp"] =
                        epoch_to_iso8601(co_await entry.update_timestamp());

                    // If the event is defined in the redfish registry, map
                    // it appropriately.
                    if (auto def = event_defs.find(message);
                        def != std::end(event_defs))
                    {
                        using event_t = utils::redfish_registry::event_t;
                        auto redfish = json::empty_map();

                        redfish["id"] = std::get<event_t>(*def).id;

                        std::vector<std::string> args = {};
                        for (const auto& arg : std::get<event_t>(*def).args)
                        {
                            if (additional_data.contains(arg))
                            {
                                args.emplace_back(additional_data[arg]);
                            }
                            else
                            {
                                args.emplace_back("");
                            }
                        }
                        redfish["args"] = std::move(args);

                        entry_json["redfish"] = std::move(redfish);
                    }
                }
                catch (...)
                {
                    warning("Failed to parse log entry: {PATH}", "PATH",
                            path.str);
                }
            });

        json::display(result);
        co_return;
    }

    bool arg_unresolved_only = false;
};
MFGTOOL_REGISTER(command);
} // namespace mfgtool::cmds::log_display

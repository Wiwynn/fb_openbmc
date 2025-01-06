#include "utils/dbus.hpp"
#include "utils/json.hpp"
#include "utils/mapper.hpp"
#include "utils/register.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/message.hpp>

#include <format>

namespace mfgtool::cmds::postcode_display
{

PHOSPHOR_LOG2_USING;
using namespace dbuspath;

struct command
{
    void init(CLI::App& app)
    {
        auto cmd = app.add_subcommand("postcode-display", "Display postcodes.");
        cmd->add_option("-p,--position", arg_pos, "Device position")
            ->required();

        init_callback(cmd, *this);
    }

    auto run(sdbusplus::async::context& ctx) -> sdbusplus::async::task<>
    {
        std::vector<std::string> postcodes{};

        auto path = host::postcode::path(arg_pos);

        auto service = co_await utils::mapper::object_service(
            ctx, path, dbuspath::host::postcode::interface);

        if (!service)
        {
            warning("Cannot find postcode service for position {POSITION}.",
                    "POSITION", arg_pos);

            json::display(postcodes);
            co_return;
        }

        info("Getting postcode entries from {SERVICE}.", "SERVICE", *service);
        auto proxy = host::postcode::Proxy(ctx).service(*service).path(path);
        auto raw_postcodes = co_await proxy.get_post_codes(1);

        // Insert the formatted postcode.
        for (const auto& [code, _] : raw_postcodes)
        {
            postcodes.emplace_back(std::ranges::fold_left(
                code, std::string{}, [](const std::string& l, auto r) {
                    return l + std::format("{:02x}", r);
                }));
        }

        json::display(postcodes);
        co_return;
    }

    size_t arg_pos = 0;
};
MFGTOOL_REGISTER(command);

} // namespace mfgtool::cmds::postcode_display

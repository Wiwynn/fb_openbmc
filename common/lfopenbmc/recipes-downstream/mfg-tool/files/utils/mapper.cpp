#include "utils/mapper.hpp"

namespace mfgtool::utils::mapper
{

namespace details
{

static inline auto subtree(sdbusplus::async::context& ctx, const auto& subpath,
                           const auto& interface, size_t depth = 0)
{
    using ObjectMapper =
        sdbusplus::client::xyz::openbmc_project::ObjectMapper<>;

    auto mapper = ObjectMapper(ctx)
                      .service(ObjectMapper::default_service)
                      .path(ObjectMapper::instance_path);

    return mapper.get_sub_tree(subpath, depth, {interface});
}

static inline auto object_service(sdbusplus::async::context& ctx,
                                  const auto& path, const auto& interface)
{
    using ObjectMapper =
        sdbusplus::client::xyz::openbmc_project::ObjectMapper<>;

    auto mapper = ObjectMapper(ctx)
                      .service(ObjectMapper::default_service)
                      .path(ObjectMapper::instance_path);

    return mapper.get_object(path, {interface});
}

} // namespace details

auto subtree_services(sdbusplus::async::context& ctx,
                      const std::string& subpath, const std::string& interface,
                      size_t depth) -> sdbusplus::async::task<services_t>
{
    auto objects = co_await details::subtree(ctx, subpath, interface, depth);
    services_t services{};

    for (const auto& [objPath, objServices] : objects)
    {
        for (const auto& [service, _] : objServices)
        {
            services[objPath].push_back(service);
        }
    }
    co_return services;
}

auto subtree_for_each(
    sdbusplus::async::context& ctx, const std::string& subpath,
    const std::string& interface,
    const std::function<sdbusplus::async::task<>(
        const sdbusplus::message::object_path&, const std::string&)>& coroutine,
    size_t depth) -> sdbusplus::async::task<>
{
    PHOSPHOR_LOG2_USING;

    auto objects = co_await subtree_services(ctx, subpath, interface, depth);

    debug("Iterating over entries.");
    for (const auto& [path, services] : objects)
    {
        if (services.size() > 1)
        {
            warning("Multiple services ({COUNT}) provide {PATH}.", "PATH", path,
                    "COUNT", services.size());
            for (const auto& s : services)
            {
                warning("Service available at {SERVICE}.", "SERVICE", s);
            }
        }

        info("Examining {INTERFACE} at {PATH}.", "INTERFACE", interface, "PATH",
             path);
        co_await coroutine(path, services[0]);
    }
}

auto subtree_for_each_interface(
    sdbusplus::async::context& ctx, const std::string& subpath,
    const std::string& interface,
    const std::function<sdbusplus::async::task<>(
        const std::string&, const std::string&, const std::string&)>& coroutine,
    size_t depth) -> sdbusplus::async::task<>
{
    PHOSPHOR_LOG2_USING;

    debug("Looking up objects under {PATH}.", "PATH", subpath);
    auto objects = co_await details::subtree(ctx, subpath, interface, depth);

    debug("iterating over entries.");
    for (const auto& [path, services] : objects)
    {
        for (const auto& [service, interfaces] : services)
        {
            for (const auto& iface : interfaces)
            {
                info("Examining {INTERFACE} at {PATH} by {SERVICE}",
                     "INTERFACE", interface, "PATH", path, "SERVICE", service);
                co_await coroutine(path, service, iface);
            }
        }
    }
}

auto object_service(sdbusplus::async::context& ctx, const std::string& path,
                    const std::string& interface)
    -> sdbusplus::async::task<std::optional<std::string>>
{
    // Mapper look up will return an exception of ResourceNotFound if the path
    // doesn't exist.  Catch the exception and turn it into a nullopt.
    try
    {
        auto result = co_await details::object_service(ctx, path, interface);
        co_return result.begin()->first;
    }
    catch (...)
    {
        co_return std::nullopt;
    }
}

} // namespace mfgtool::utils::mapper

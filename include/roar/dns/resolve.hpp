#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <stdexcept>
#include <algorithm>
#include <vector>

namespace Roar::Dns
{
    /**
     * @brief Resolves to a set of possible endpoints. Useful for clients that can try all and connect to one that
     * works.
     *
     * @tparam Protocol ip/udp
     * @tparam FunctionT Type of function to call with each endpoint.
     * @param context Io executor.
     * @param host The host to resolve
     * @param port The port to resolve
     * @param onResolveDone Function that is called with all results.
     * @param flags Flags to pass to the resolver.
     */
    template <typename Protocol = boost::asio::ip::tcp, typename FunctionT>
    void resolveAll(
        boost::asio::any_io_executor context,
        std::string const& host,
        std::string const& port,
        FunctionT&& onResolveDone,
        boost::asio::ip::resolver_base::flags flags = {})
    {
        using ResolverType = typename Protocol::resolver;
        ResolverType resolver{context};

        boost::system::error_code ec;
        const auto start = resolver.resolve(host.c_str(), port.c_str(), flags, ec);
        const auto end = decltype(start){};

        if (start == end || ec)
            throw std::runtime_error("Cannot resolve passed host.");

        onResolveDone(start, end);
    }

    /**
     * @brief Resolves to a set of possible endpoints. Useful for clients that can try all and connect to one that
     * works. Does not throw, but rather sets ec.
     *
     * @tparam Protocol ip/udp
     * @tparam FunctionT Type of function to call with each endpoint.
     * @param context Io executor.
     * @param host The host to resolve
     * @param port The port to resolve
     * @param onResolveDone Function that is called with all results.
     * @param ec Will be set to an error if resolve fails.
     * @param flags Flags to pass to the resolver.
     */
    template <typename Protocol = boost::asio::ip::tcp, typename FunctionT>
    void resolveAll(
        boost::asio::any_io_executor context,
        std::string const& host,
        std::string const& port,
        FunctionT&& onResolveDone,
        boost::system::error_code& ec,
        boost::asio::ip::resolver_base::flags flags = {})
    {
        using ResolverType = typename Protocol::resolver;
        ResolverType resolver{context};

        const auto start = resolver.resolve(host.c_str(), port.c_str(), flags, ec);
        const auto end = decltype(start){};

        if (start == end || ec)
            return;

        onResolveDone(start, end);
    }

    /**
     * @brief See other overload, this one just takes an unsigned short.
     * @param flags Flags to pass to the resolver.
     */
    template <typename Protocol = boost::asio::ip::tcp, typename FunctionT>
    void resolveAll(
        boost::asio::any_io_executor context,
        std::string const& host,
        unsigned short port,
        FunctionT&& onResolveDone,
        boost::system::error_code& ec,
        boost::asio::ip::resolver_base::flags flags = {})
    {
        resolveAll(
            context,
            host,
            port,
            [&onResolveDone](auto begin, auto end) {
                onResolveDone(typename Protocol::resolver::iterator{begin}, typename Protocol::resolver::iterator{end});
            },
            ec,
            flags);
    }

    /**
     * @brief Resolves to a set of possible endpoints. Useful for clients that can try all and connect to one that
     * works.
     *
     * @tparam Protocol ip/udp
     * @tparam FunctionT Type of function to call with each endpoint.
     * @param context Io executor.
     * @param host The host to resolve
     * @param port The port to resolve
     * @param onResolveDone Function that is called with all results.
     * @param flags Flags to pass to the resolver.
     */
    template <typename Protocol = boost::asio::ip::tcp>
    void resolveAll(
        boost::asio::any_io_executor context,
        std::string const& host,
        std::string const& port,
        std::function<void(typename Protocol::resolver::iterator, typename Protocol::resolver::iterator)> onResolveDone,
        boost::asio::ip::resolver_base::flags flags = {})
    {
        resolveAll(
            context,
            host,
            port,
            [&onResolveDone](auto begin, auto end) {
                onResolveDone(typename Protocol::resolver::iterator{begin}, typename Protocol::resolver::iterator{end});
            },
            flags);
    }

    /**
     * @brief Resolves a host and port and calls the passed function with all results and then returns what picker
     * returns.
     *
     * @tparam ExecutorType Deduced. The type of the io executor (likely any_io_executor)
     * @tparam PickerFunctionT A function that takes a vector<typename ResolverType::endpoint_type> and returns
     * typename ResolverType::endpoint_type
     * @tparam Protocol ip or udp?
     * @param executor Any executor type that boost::asio::ip::PROTOCOL::resolver can take.
     * @param host The host to resolve
     * @param port The port to resolve
     * @param picker A picker function.
     * @return Protocol::resolver::endpoint_type A picked endpoint by the picker function.
     * @param flags Flags to pass to the resolver.
     */
    template <typename ExecutorType, typename PickerFunctionT, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type resolveSelect(
        ExecutorType&& executor,
        std::string const& host,
        std::string const& port,
        PickerFunctionT&& picker,
        boost::asio::ip::resolver_base::flags flags = {})
    {
        using ResolverType = typename Protocol::resolver;
        ResolverType resolver{executor};

        boost::system::error_code ec;
        auto result = resolver.resolve(host.c_str(), port.c_str(), flags, ec);
        std::vector<typename ResolverType::endpoint_type> endpoints{result.begin(), result.end()};

        if (endpoints.empty())
            throw std::runtime_error("Cannot resolve passed host.");

        return picker(endpoints);
    }

    /**
     * @brief Resolves a host and port and calls the passed function with all results and then returns what picker
     * returns.
     * See the other overload. This one only differs by the port, which is an unsigned short.
     * @param flags Flags to pass to the resolver.
     */
    template <typename ExecutorType, typename PickerFunctionT, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type resolveSelect(
        ExecutorType&& executor,
        std::string const& host,
        unsigned short port,
        PickerFunctionT&& picker,
        boost::asio::ip::resolver_base::flags flags = {})
    {
        return resolveSelect<ExecutorType, PickerFunctionT, Protocol>(
            std::forward<ExecutorType>(executor),
            host,
            std::to_string(port),
            std::forward<PickerFunctionT>(picker),
            flags);
    }

    /**
     * @brief Like resolve but picks one of the addresses. Will sort after ipv4/ipv6
     *
     * @tparam ExecutorType Deduced. The type of the io executor (likely any_io_executor)
     * @tparam Protocol ip or udp?
     * @param executor Any executor type that boost::asio::ip::PROTOCOL::resolver can take.
     * @param host The host to resolve
     * @param port The port to resolve
     * @param preferIpv4 Prefer to pick an ipv4 address? (Default is false)
     * @param flags Flags to pass to the resolver.
     * @return Protocol::resolver::endpoint_type Returns one of the results.
     */
    template <typename ExecutorType, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type resolveSingle(
        ExecutorType&& executor,
        std::string const& host,
        std::string const& port,
        bool preferIpv4 = false,
        boost::asio::ip::resolver_base::flags flags = {})
    {
        return resolveSelect(
            std::forward<ExecutorType>(executor),
            host,
            port,
            [&preferIpv4](auto& endpoints) {
                std::partition(std::begin(endpoints), std::end(endpoints), [preferIpv4](auto const& lhs) {
                    return lhs.address().is_v4() == preferIpv4;
                });
                return endpoints[0];
            },
            flags);
    }

    /**
     * @brief This overload only differs from the other one by taking an unsigned short as the port.
     * @param flags Flags to pass to the resolver.
     */
    template <typename ExecutorType, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type resolveSingle(
        ExecutorType&& executor,
        std::string const& host,
        unsigned short port,
        bool preferIpv4 = false,
        boost::asio::ip::resolver_base::flags flags = {})
    {
        return resolveSingle<ExecutorType, Protocol>(
            std::forward<ExecutorType>(executor), host, std::to_string(port), preferIpv4, flags);
    }
}
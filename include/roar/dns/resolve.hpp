#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <stdexcept>
#include <algorithm>
#include <vector>

namespace Roar::Dns
{
    /**
     * @brief Resolves a host and port and calls the passed function with all results and then returns what picker
     * returns.
     *
     * @tparam ExecutorType Deduced. The type of the io executor (likely any_io_executor)
     * @tparam PickerFunctionT A function that takes a vector<typename ResolverType::endpoint_type> and returns typename
     * ResolverType::endpoint_type
     * @tparam Protocol ip or udp?
     * @param executor Any executor type that boost::asio::ip::PROTOCOL::resolver can take.
     * @param host The host to resolve
     * @param port The port to resolve
     * @param picker A picker function.
     * @return Protocol::resolver::endpoint_type A picked endpoint by the picker function.
     */
    template <typename ExecutorType, typename PickerFunctionT, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type
    resolve(ExecutorType&& executor, std::string const& host, std::string const& port, PickerFunctionT&& picker)
    {
        using ResolverType = typename Protocol::resolver;
        ResolverType resolver{executor};

        boost::system::error_code ec;
        typename ResolverType::iterator end, start = resolver.resolve(host.c_str(), port.c_str(), ec);
        std::vector<typename ResolverType::endpoint_type> endpoints(start, end);

        if (endpoints.empty())
            throw std::runtime_error("Cannot resolve passed host.");

        return picker(endpoints);
    }

    /**
     * @brief Resolves a host and port and calls the passed function with all results and then returns what picker
     * returns.
     * See the other overload. This one only differs by the port, which is an unsigned short.
     */
    template <typename ExecutorType, typename PickerFunctionT, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type
    resolve(ExecutorType&& executor, std::string const& host, unsigned short port, PickerFunctionT&& picker)
    {
        return resolve<ExecutorType, PickerFunctionT, Protocol>(
            std::forward<ExecutorType>(executor), host, std::to_string(port), std::forward<PickerFunctionT>(picker));
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
     * @return Protocol::resolver::endpoint_type Returns one of the results.
     */
    template <typename ExecutorType, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type
    resolveSingle(ExecutorType&& executor, std::string const& host, std::string const& port, bool preferIpv4 = false)
    {
        return resolve(std::forward<ExecutorType>(executor), host, port, [&preferIpv4](auto& endpoints) {
            std::partition(std::begin(endpoints), std::end(endpoints), [preferIpv4](auto const& lhs) {
                return lhs.address().is_v4() == preferIpv4;
            });
            return endpoints[0];
        });
    }

    /**
     * @brief This overload only differs from the other one by taking an unsigned short as the port.
     */
    template <typename ExecutorType, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type
    resolveSingle(ExecutorType&& executor, std::string const& host, unsigned short port, bool preferIpv4 = false)
    {
        return resolveSingle<ExecutorType, Protocol>(
            std::forward<ExecutorType>(executor), host, std::to_string(port), preferIpv4);
    }
}
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <stdexcept>
#include <algorithm>
#include <vector>

namespace Roar::Dns
{
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
    template <typename ExecutorType, typename PickerFunctionT, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type
    resolve(ExecutorType&& executor, std::string const& host, unsigned short port, PickerFunctionT&& picker)
    {
        return resolve<ExecutorType, PickerFunctionT, Protocol>(
            std::forward<ExecutorType>(executor), host, std::to_string(port), std::forward<PickerFunctionT>(picker));
    }

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

    template <typename ExecutorType, typename Protocol = boost::asio::ip::tcp>
    typename Protocol::resolver::endpoint_type
    resolveSingle(ExecutorType&& executor, std::string const& host, unsigned short port, bool preferIpv4 = false)
    {
        return resolveSingle<ExecutorType, Protocol>(
            std::forward<ExecutorType>(executor), host, std::to_string(port), preferIpv4);
    }
}
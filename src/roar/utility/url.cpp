#include <roar/utility/url.hpp>

#include <iterator>
#include <sstream>
#include <utility>

namespace Roar
{
    //##################################################################################################################
    std::string Url::toString() const
    {
        std::stringstream result;
        result << toShortString();
        result << path;
        if (!query.empty())
        {
            result.put('?');
            for (auto iter = std::begin(query), end = std::end(query); iter != end; ++iter)
            {
                result << iter->first << '=' << iter->second;
                if (std::next(iter) != end)
                    result << '&';
            }
        }
        return result.str();
    }
    //------------------------------------------------------------------------------------------------------------------
    std::string Url::toShortString() const
    {
        std::stringstream result;
        result << scheme << "://";
        if (userInfo)
            result << userInfo->user << ':' << userInfo->password << '@';

        result << remote.host;
        if (remote.port)
            result << ':' << *remote.port;
        return result.str();
    }
    //##################################################################################################################
}

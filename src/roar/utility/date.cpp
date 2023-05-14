#include <roar/utility/date.hpp>

#include <sstream>
#include <iomanip>

namespace Roar
{
    // #####################################################################################################################
    std::string tmFormatter(std::tm* tm, const char* suffix = nullptr)
    {
        static constexpr const char* weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        static constexpr const char* months[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

        std::stringstream sstr;
        sstr << weekdays[tm->tm_wday] << ", " << tm->tm_mday << " " << months[tm->tm_mon] << ' ' << (1900 + tm->tm_year)
             << " " << std::setfill('0') << std::setw(2) << tm->tm_hour << ':' << std::setfill('0') << std::setw(2)
             << tm->tm_min << ':' << std::setfill('0') << std::setw(2) << tm->tm_sec;
        if (suffix != nullptr)
            sstr << suffix;
        return sstr.str();
    }
    // #####################################################################################################################
    date::date(std::chrono::system_clock::time_point time_point)
        : timePoint_{std::move(time_point)}
    {}
    //---------------------------------------------------------------------------------------------------------------------
    date::date()
        : timePoint_{std::chrono::system_clock::now()}
    {}
    //---------------------------------------------------------------------------------------------------------------------
    std::chrono::system_clock::time_point& date::getTimePoint()
    {
        return timePoint_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::string date::toGmtString() const
    {
        auto time = std::chrono::system_clock::to_time_t(timePoint_);
#ifdef _MSC_VER
        tm tmTime{};
        gmtime_s(&tmTime, &time);
        return tmFormatter(&tmTime, " GMT");
#else
        return tmFormatter(gmtime(&time), " GMT");
#endif
    }
    // #####################################################################################################################
}

#pragma once

#include <string>

namespace Roar
{
    struct WebsocketReadResult
    {
        /// Is received data binary?
        bool isBinary;
        bool isMessageDone;
        std::string message;
    };
}
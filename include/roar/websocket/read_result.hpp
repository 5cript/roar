#pragma once

#include <string>

namespace Roar
{
    struct WebSocketReadResult
    {
        /// Is received data binary?
        bool isBinary;
        bool isMessageDone;
        std::string message;
    };
}
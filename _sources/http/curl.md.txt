# Curl

Roar provides a wrapper around libcurl.
The most important object is the <a href="/roar/doxygen/classRoar_1_1Curl_1_1Request.html">Curl::Request</a>.
With that you can build requests and "curl_perform" them.
Reference the doxygen documentation for details, but here is an example:

```{code-block} c++
---
lineno-start: 1
caption: Curl::Request
---
#include <roar/curl/request.hpp>
#include <iostream>

int main()
{
    std::string responseBody;
    const auto response = Roar::Curl::Request{}
        .sink(responseBody)
        .verbose()
        .setHeaderField("Content-Type", "text/plain")
        .source("Hi")
        .post("https://localhost:8080")
    ;

    std::cout << responseBody << "\n";
    return response.code();
}
```

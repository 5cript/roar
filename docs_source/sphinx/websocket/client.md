# Websocket Client

## Example

```{code-block} c++
---
lineno-start: 1
caption: A class that gives access to the filesystem
---
auto client = std::make_shared<WebsocketClient>(
    WebsocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt});
client
    ->connect({
        .host = "localhost",
        .port = std::to_string(server_->getLocalEndpoint().port()),
        .path = "/",
    })
    .then([client]() {
        client->binary(false);
        client->send("Hello You!").then([](std::size_t){
            // sent.
        });
    })
    .fail([](auto) {
        // Could not connect.
    });
```
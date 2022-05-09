# ROAR

Github Pages Documentation: https://5cript.github.io/roar/

This is a network library that has:
- a HTTP(S) Server based on boost beast.
- a CURL easy wrapper http client.
- Javascript Promise like syntax for asynchronous actions provided by xhawk18_promise.

## Features and planed features

- [x] HTTP Server
  - [x] HTTPS Support (facillitated using boost::asio::ssl_context)
    - [x] Hybrid support, allow marked requests as unsecure.
  - [x] Routing (verb+path => handler)
  - [x] Custom Error Pages (404, ...)
  - [x] Regex paths
  - [x] Serving files (easier than using regex paths)
    - [x] Download
    - [x] Upload
    - [x] Deletion
    - [x] Directory Listing
    - [ ] Resumeable requests (Range Requests).
- [x] Websocket
  - [x] Websocket Server Support (SSL and Plain)
  - [x] Websocket Client (SSL and Plain)
  - [x] Websocket Upgrade from HTTP(S) Server
- [x] Synchronous HTTP Client using libcurl.
- [ ] URL Parser
- [ ] HTTP & HTTPS Tunnel Proxy

# Developer Notes
On msys2 use python (3) and python-pip of your subsystem, otherwise packages are not found
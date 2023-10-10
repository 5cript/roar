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
  - [x] Custom error pages (404, ...)
  - [ ] Provided pretty standard replies / error pages.
  - [x] Regex paths
  - [x] Serving files (easier than using regex paths)
    - [x] Download
    - [x] Upload
    - [x] Deletion
    - [x] Directory Listing
      - [x] List of files
      - [x] Styleable, pretty by default
      - [x] Last modified date
      - [x] File size
    - [x] Range Requests for GET.
    - [ ] Resumeable Uploads?
- [x] Websocket
  - [x] Websocket Server Support (SSL and Plain)
  - [x] Websocket Client (SSL and Plain)
  - [x] Websocket Upgrade from HTTP(S) Server
- [x] Synchronous HTTP Client using libcurl.
- [x] URL Parser
- [ ] HTTP & HTTPS Tunnel Proxy

## Dependencies

- boost 1.81.0 or higher
    - openssl::ssl
    - openssl::crypto
    - cryptopp
- libcurl

### MacOS

Dependencies are expected to be installed via brew.
If this does not work for you, please open an issue.

brew install ninja boost cryptopp curl llvm@16

### Windows

Use vcpkg on windows for these dependencies when building with Visual Studio & cmake.
https://vcpkg.io/en/getting-started.html

Example CMakeSettings.json
```json
{
  "configurations": [
    {
      "name": "x64-Clang-Debug",
      "generator": "Ninja",
      "configurationType": "Debug",
      "buildRoot": "${projectDir}\\build\\${name}",
      "installRoot": "${projectDir}\\build\\install\\${name}",
      "buildCommandArgs": "",
      "ctestCommandArgs": "",
      "inheritEnvironments": [ "clang_cl_x64" ],
      "cmakeCommandArgs": "",
      "cmakeToolchain": "D:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    }
  ]
}
```

# Developer Notes
On msys2 use python (3) and python-pip of your subsystem, otherwise packages are not found
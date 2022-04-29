#!/usr/bin/python3

import os
import sys
import time
import threading
import webbrowser
from http.server import HTTPServer, SimpleHTTPRequestHandler


def start_server(ip, port):
    server_address = (ip, port)
    httpd = HTTPServer(server_address, SimpleHTTPRequestHandler)
    httpd.serve_forever()


def main():
    ip = "127.0.0.1"
    port = 8080
    url = f"http://{ip}:{port}/home.html"

    print ("changing dir")
    # cd to the script
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    print ("starting thread")
    threading.Thread(target=start_server, args=(ip, port)).start()
    webbrowser.open_new(url)
    print ("loop")
    while True:
        try:
            time.sleep(0.1)
        except KeyboardInterrupt:
            sys.exit(0)


if __name__ == "__main__":
    main()

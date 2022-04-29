#!/usr/bin/env python3
"""
Postprocesses all SVG files and removes bogus opening template bracket from collab graphs
"""

import os
import re
import argparse


def postprocessSvg(fileName):
    fileContent = []
    with open(fileName) as file:
        for line in file:
            line = re.sub(r'&lt;</text>', '</text>', line)
            fileContent.append(line)

    with open(fileName, 'w') as file:
        for line in fileContent:
            file.write(line)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-i', '--input', help="Provide doxygen html directory", type=str)
    args = parser.parse_args()
    for filename in os.listdir(args.input):
        if filename.endswith(".svg"):
            print("Postprocessing svg: ", filename)
            postprocessSvg(os.path.join(args.input, filename))


if __name__ == "__main__":
    main()

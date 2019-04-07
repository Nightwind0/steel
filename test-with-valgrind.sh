#!/bin/bash

echo -e "!!! Testing script '$1' (if the script is io.steel, you'll need to type something and hit enter before it continues) ...\n"
valgrind -v --leak-check=full ./steeli "$1" &> "$1.valgrind.log" || echo "\n!!! FAILED\n\n"

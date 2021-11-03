#!/bin/bash

[[ "$OSTYPE" == "msys" ]]&&cmake -G"MSYS Makefiles" .
[[ "$OSTYPE" == "darwin"* ]]&&cmake .
#!/usr/bin/env bash

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    COMPILER="g++"
    OUTPUT="main.exe"
else
    COMPILER="clang++"
    OUTPUT="main"
fi

echo "Using $COMPILER on $OSTYPE..."

rm -f $OUTPUT

$COMPILER main.cpp -o $OUTPUT

if [ $? -eq 0 ]; then
    ./$OUTPUT bpsndb.db
else 
    echo "Compilation failed"
fi

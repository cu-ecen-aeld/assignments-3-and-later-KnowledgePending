#!/bin/sh
# Assignment 1 writer script
# Author: Bryan Flood

if [ $# -ne 2 ]; then
    echo "Error: program requires 2 args, writefile and writestr"
    exit 1
fi

writefile="$1"
writestr="$2"

mkdir -p "$(dirname "$writefile")" || {
    echo "Error: could not create directory"
    exit 1
}

echo "$writestr" > "$writefile" || {
    echo "Error: could not write to file"
    exit 1
}

exit 0
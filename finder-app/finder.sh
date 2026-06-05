#!/bin/sh
# Assignment 1 script
# Author: Bryan Flood

if [ $# -ne 2 ]; then
    echo "Error: program requires 2 args, filesdir and searchstr"
    exit 1
fi

filesdir="$1"
searchstr="$2"

if [ ! -d "$filesdir" ]; then
    echo "Error: filesdir is not a directory"
    exit 1
fi

filecount=$(find "$filesdir" -type f | wc -l)
matchcount=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $filecount and the number of matching lines are $matchcount"
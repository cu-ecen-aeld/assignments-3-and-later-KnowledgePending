#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

username=$(cat /etc/finder-app/conf/username.txt)
assignment=$(cat /etc/finder-app/conf/assignment.txt)

if [ $# -lt 3 ]
then
    echo "Using default value ${WRITESTR} for string to write"
    if [ $# -lt 1 ]
    then
        echo "Using default value ${NUMFILES} for number of files to write"
    else
        NUMFILES=$1
    fi
else
    NUMFILES=$1
    WRITESTR=$2
    WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

if [ "$assignment" != "assignment1" ]
then
    mkdir -p "$WRITEDIR"
fi

for i in $(seq 1 $NUMFILES)
do
    # Use PATH for installed binaries
    writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")

# Write output to required result file for Assignment 4
echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt

# Clean up temporary files
rm -rf /tmp/aeld-data

# Check expected output
echo "${OUTPUTSTRING}" | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed"
    exit 1
fi
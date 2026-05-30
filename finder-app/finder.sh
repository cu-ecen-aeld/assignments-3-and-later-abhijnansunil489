#! /bin/sh

if [ ! $# -eq 2 ]
then
	echo "ERROR: ONLY 2 ARGUMENTS ALLOWED"
	exit 1
elif [ ! -d $1 ]
then
	echo "ERROR: ARG1 NOT A PATH TO DIRECTORY"
	exit 1
else
	X=$(find "$1" -type f | wc -l)
	Y=$(grep -r "$2" "$1" | wc -l)
	echo "The number of files are $X and the number of matching lines are $Y"
	exit 0
fi



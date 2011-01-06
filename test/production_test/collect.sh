#!/bin/sh


# assume source data in current dir.

index=0

while [ $index -lt 24 ]; do
	echo "loop: $index"
	if [ ! -e response${index}.xml ]; then
		cpi -r query${index}.xml -w response${index}.xml
		echo "sleeping..."
		sleep 300
	fi
	index=$(expr $index + 1)
done


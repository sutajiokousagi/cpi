#!/bin/sh
# install.sh - top-level export installer
#
# This is invoked with TARGET and CNPLATFORM defined in the environment
# (e.g. TARGET=arm-linux CNPLATFORM=ironforge)
# and a required argument which is the location of rfs1 and rfs2, e.g.
#
# TARGET=arm-linux CNPLATFORM=ironforge ./install.sh ../../partitions
#

# Check for required args
[ "${TARGET}" = "" -o "${CNPLATFORM}" = "" -o "$1" = "" ] && {
	echo "$0: unable to copy, either TARGET and CNPLATFORM not specified or destination dir not specified"
	exit 1
}
TARGETDIR=${TARGET}-${CNPLATFORM}

# Check for valid targets
VALID_TARGET_LIST="${TARGET} x86-linux"
VALID_TARGET=
for target in ${VALID_TARGET_LIST}
do
  [ "${target}" = "${TARGET}" ] && VALID_TARGET=1
done
[ "${VALID_TARGET}" ] || {
	echo "Target ${TARGET} is not valid. Valid targets are:"
	echo "${VALID_TARGET_LIST}"
	exit 1
}

PROJBASE=$(dirname $0)
COPYDEST=$1

# Check for rfs1 and rfs2 dirs
[ -d ${COPYDEST}/rfs1 -a -d ${COPYDEST}/rfs2 ] || {
	echo "$0: ${COPYDEST}/rfs1 and rfs2 not found, cannot continue"
	exit 1
}

echo "Starting copy from ${PROJBASE} to ${COPYDEST}/rfs1"
# Copy all binaries
cp --verbose --preserve ${PROJBASE}/${TARGETDIR}/bin/cpi ${COPYDEST}/rfs1/usr/bin/

# No libs
#cp --verbose --preserve --no-dereference ${PROJBASE}/${TARGETDIR}/lib/*.so* ${COPYDEST}/rfs1/lib

echo "Starting copy from ${PROJBASE} to ${COPYDEST}/rfs2"
cp --verbose --preserve ${PROJBASE}/${TARGETDIR}/bin/cpi ${COPYDEST}/rfs2/usr/bin/

echo "Copy from ${PROJBASE} completed."


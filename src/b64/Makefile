# $Id$
# wrapper makefile for libb64
# tarball downloaded from http://synesis.com.au/software/b64.html
#

OUTPUT_BASE:=$(abspath ../../output/${TARGET}-${CNPLATFORM}/b64)
SRC_ROOT=$(lastword $(wildcard src/b64*))
# This works with 4.1.2 as well as with the newer toolchain
GCC_VER=gcc43
ifeq (${TARGET},)
TGT=
else
TGT=${TARGET}-
endif

build: unpack
	make -C ${SRC_ROOT}/build/${GCC_VER} \
		-f makefile.unix \
		CC=${TGT}gcc CPPC=${TGT}g++ LD=${TGT}g++ AR=${TGT}ar \
		COMP_TAG=${TARGET} \
		CC_F_WARNING_ERR= \
		CPPC_F_WARNING_ERR= \
		build.libs.core
	touch $@

install: build ${OUTPUT_BASE}/include ${OUTPUT_BASE}/lib
	cp --preserve --no-dereference --recursive ${SRC_ROOT}/include/b64 ${OUTPUT_BASE}/include/
	cp --preserve --no-dereference ${SRC_ROOT}/lib/libb64.${TARGET}.a ${OUTPUT_BASE}/lib/

clean:
	-rm -rf src unpack build ${OUTPUT_BASE}

${OUTPUT_BASE}/include ${OUTPUT_BASE}/lib:
	mkdir -p $@

unpack: ${TARBALL}
ifeq (${TARBALL},)
	@echo "TARBALL not specified"
	@exit 1
else
	-rm -rf src
	mkdir src
	cd src ; unzip $(abspath ${TARBALL})
	touch $@
endif

.PHONY: 


# $Id$
#
# CPI Makefile
#
# Crypto Processor Interface
#

ifeq (${METAPROJECT_ROOT},)
export METAPROJECT_ROOT:=$(abspath ../..)
endif

export CNPLATFORM=ironforge
export TARGET=arm-linux
export PLATFORM_TARGET=$(TARGET)-$(CNPLATFORM)
CROSS_COMPILE=
ifneq ($(TARGET),)
CROSS_COMPILE=$(TARGET)-
endif

include ${METAPROJECT_ROOT}/config/config.mk


CFLAGS   =-Wall -g -DLTM_DESC -DTFM_DESC -I ../include -I../output/${PLATFORM_TARGET}/b64/include -I../import/libs/all/all/include -DCNPLATFORM_$(CNPLATFORM)
LDFLAGS  = -pthread -lb64.$(TARGET) -lexpat -ltomcrypt -ltommath -ltfm -L../output/${PLATFORM_TARGET}/b64/lib -L../import/libs/${TARGET}/lib
OUT_DIR  = ../output/$(PLATFORM_TARGET)
OUT_BIN  = $(OUT_DIR)/bin/cpi
OUT_TST  = $(OUT_DIR)/bin/test
OUT_LIB  = $(OUT_DIR)/lib/libcpi.a
OUT_INC  = ../include/*.h
OUT_DOC  = ../doc/doxygen
CFG_DOC  = ../doc/doxygen.conf
EXP_DIR  = ../export/$(PLATFORM_TARGET)
EXP_BIN  = $(EXP_DIR)/bin/cpi
EXP_LIB  = $(EXP_DIR)/lib/libcpi.a
EXP_INC  = ../export/all/all/include/
EXP_DOC  = ../export/all/all/doc/
OUT_DIRS = $(dir $(OUT_BIN) $(OUT_LIB) $(EXP_BIN) $(EXP_LIB))
SOURCES  = $(wildcard ../src/*.c)
OBJS     = $(SOURCES:.c=.o)
CC       = $(CROSS_COMPILE)gcc
CXX      = $(CROSS_COMPILE)g++
STRIP    = $(CROSS_COMPILE)strip
AR       = $(CROSS_COMPILE)ar
DOXYGEN  = doxygen

# Set DIFFREV=-r2798:2833 to compare specific revisions
DIFFREV=
# Set DIFFDIR=../src to compare only source
DIFFDIR=..

all: $(OUT_DIRS) $(OUT_LIB) $(OUT_BIN) $(OUT_TST)

$(OUT_DIRS):
	@echo "Creating dir $@"
	-mkdir -p $@

.c.o:
	@echo "  C $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OUT_LIB): b64 $(OBJS)
	@echo "  A $(OUT_LIB)"
	@$(AR) rcs $(OUT_LIB) ../src/*.o

$(OUT_BIN): b64 $(OBJS) ../src/cmdline/main.o
	@echo "  B $(OUT_BIN)"
	@$(CC) ../src/*.o ../src/cmdline/main.o $(LDFLAGS) -o $@
	@$(STRIP) -d $@

$(OUT_TST): b64 $(OBJS) ../test/src/main.o
	@echo "  B $(OUT_TST)"
	@$(CC) ../src/*.o ../test/src/main.o $(LDFLAGS) -o $@
	@$(STRIP) -d $@

b64:
	@echo "  M b64"
	${MAKE} -C ../src/b64 TARBALL=$(abspath $(lastword $(wildcard ../src/.tarballs/b64-*.zip))) install

doxygen:
	@echo "  D $(CFG_DOC)"
	@mkdir -p $(OUT_DOC)
	@$(DOXYGEN) $(CFG_DOC) 1 > /dev/null

clean:
	@echo "  X $(OUT_TST)"
	@-rm -rf $(OUT_TST)
	@echo "  X ../test/src/*.o"
	@-rm -rf ../test/src/*.o
	@echo "  X $(OUT_BIN)"
	@-rm -rf $(OUT_BIN)
	@echo "  X ../src/cmdline/*.o"
	@-rm -rf ../src/cmdline/*.o
	@echo "  X $(OUT_LIB)"
	@-rm -rf $(OUT_LIB)
	@echo "  X ../src/*.o"
	@-rm -rf ../src/*.o
	@echo "  X ../src/b64"
	${MAKE} -C ../src/b64 clean

exports: all exports-lib exports-bin
	cp --preserve install.sh ../export

ifneq ($(SKIP_DOXYGEN),1)
exports: doxygen exports-doxygen
endif

exports-lib:
	@echo "  E $(EXP_LIB)"
	@-cp $(OUT_LIB) $(EXP_LIB)
	@echo "  E $(EXP_INC)"
	@-cp $(OUT_INC) $(EXP_INC)

exports-bin:
	@echo "  E $(EXP_BIN)"
	@-cp $(OUT_BIN) $(EXP_BIN)

exports-doxygen:
	@echo "  E $(EXP_DOC)"
	@-cp -r $(OUT_DOC) $(EXP_DOC)

exports-clean: exports-clean-bin exports-clean-lib

ifneq ($(SKIP_DOXYGEN),1)
exports-clean: exports-clean-doc
endif

exports-clean-bin:
	@echo "  X $(EXP_BIN)"
	@-rm -rf $(EXP_BIN)

exports-clean-lib:
	@echo "  X $(EXP_LIB)"
	@-rm -rf $(EXP_LIB)

exports-clean-doc:
	@echo "  X $(OUT_DOC)"
	@-rm -rf $(OUT_DOC)

# Build all targets required for production
build-all:
	$(MAKE) all exports PRODUCTION=1 TARGET=$(TARGET) CNPLATFORM=$(CNPLATFORM)

# Clean all production targets
clean-all:
	$(MAKE) clean TARGET=$(TARGET) CNPLATFORM=$(CNPLATFORM)
	$(MAKE) exports-clean TARGET=$(TARGET) CNPLATFORM=$(CNPLATFORM)

commit:
	export COMMIT_TIME="$(shell date +'%d-%b-%Y %H%M %Z')" ; cd ../export ; echo "Auto-commit Production=$(PRODUCTION) $${COMMIT_TIME}" >>autocommit.log ; svn commit -m"{auto} Automated export checkin by build process at $${COMMIT_TIME}"

diff:
	echo -e "cpi v$(shell awk '/#define.+VER_STR/ {print substr($$3,2,length($$3)-2);}' ../src/cmdline/main.c), $(shell date +'%d-%b-%Y %H%M %Z')\n\n* change 1...\n" >../tmp
	cd $(DIFFDIR) ; svn diff $(DIFFREV) --extensions '--unified --ignore-all-space --ignore-eol-style' >>tmp

copy:
ifneq ($(COPYCMD),)
	$(COPYCMD) ../output/$(OUT_DIR)/bin/* $(COPYDESTPREFIX)$(COPYDESTDIR)$(COPYDESTSUFFIX)
endif


.PHONY: dirs copy b64

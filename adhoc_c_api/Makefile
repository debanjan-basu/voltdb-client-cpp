
TOP_DIR:=$(shell pwd)
include $(TOP_DIR)/Make.defaults

TARGET_NAME = libcvh.so
SHARED_LIB_TARGET = $(BUILD_CONFIG)/$(TARGET_NAME)
STATIC_LIB_TARGET=$(SHARED_LIB_TARGET:.so=.a)

# Path to source files
vpath %.cpp src
vpath %.c src

SOURCESPP = voltdb_helper.cpp
SOURCES = base64.c

# Tell the Makefile where install the compiled targets ...
INSTALL_FILES = $(TARGET_BIN_DIR)/$(TARGET_NAME)
DEPEND_SUBDIRS = $(TOP_DIR)/../

INCLUDES = -I./ -I$(TOP_DIR)/../ \
	   -I$(TOP_DIR)/../include/ \

LIBS = -lvoltdbcpp
STATIC_LIBS = -levent

LIBPATHS = -L$(TOP_DIR)/../third_party_libs/linux/ \
	   -L$(TOP_DIR)/../ \


CFLAGS =
LDFLAGS =

ifeq ($(OS),centos)
endif

all-no-deps : $(SHARED_LIB_TARGET) $(STATIC_LIB_TARGET)

$(INSTALL_FILES) :

include $(TOP_DIR)/Make.app

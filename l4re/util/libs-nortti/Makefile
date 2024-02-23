PKGDIR         ?= ../..
L4DIR          ?= $(PKGDIR)/../../..

TARGET	      := lib4re-util-nortti.a lib4re-util-nortti.so
SRC_CC        := cap_alloc.cc debug.cc kumem_alloc.cc
PC_FILENAME   := l4re-util-nortti
REQUIRES_LIBS := l4re
CXXFLAGS      += -DL4_NO_RTTI -fno-rtti -fno-exceptions
PICFLAGS      += -DSHARED=1

include $(L4DIR)/mk/lib.mk

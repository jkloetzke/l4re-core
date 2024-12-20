# vi:ft=make
PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../../..

TARGET          := libc_minimal_l4re.a
PC_FILENAME     := libc_minimal_l4re
define PC_EXTRA
Link_Libs= %{static|static-pie:-lc_minimal_l4re}
endef

DEFINES         := -DL4_MINIMAL_LIBC -DL4_NO_RTTI=1
SRC_CC          := _exit.cc
REQUIRES_LIBS = l4sys l4re l4re-util libc_be_minimal_log_io
LDFLAGS         = --link-libc

vpath _exit.cc $(SRC_DIR)/../uclibc

include $(L4DIR)/mk/lib.mk

LINK_LIBC_LIBS = $(foreach dn,$(L4LIBDIR),\
                   $(foreach fn,$(addprefix $(dn)/,$(call BID_PKG_CONFIG,$(OBJ_BASE),--libs -- --link-libc,$(REQUIRES_LIBS))),\
		     $(wildcard $(strip $(fn)))))

OBJS_libc_minimal_l4re.a += $(LINK_LIBC_LIBS)

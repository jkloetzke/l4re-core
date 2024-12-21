PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../../..

TARGET      = libc_be_sig_noop.o.a libc_be_sig_noop.p.a
PC_FILENAME = libc_be_sig_noop
PC_LIBS     = %{-link-libc:%{shared:--whole-archive -lc_be_sig_noop.p --no-whole-archive;:libc_be_sig_noop.o.a}}
PC_LIBS_PIC =
SRC_CC      = sig.cc
CXXFLAGS        = -fno-exceptions

include $(L4DIR)/mk/lib.mk

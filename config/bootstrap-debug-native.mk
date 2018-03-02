# This BUILD_CONFIG option builds checks that toggling debug
# information generation doesn't affect the generated object code.

# It is very lightweight: in addition to not performing any additional
# compilation (unlike bootstrap-debug-lean), it actually speeds up
# stage2, for no debug information is generated when compiling with
# the unoptimized stage1.

# For more thorough testing, see bootstrap-debug-lean.mk

STAGE1_CFLAGS += -match=native -mtune=native
STAGE2_CFLAGS += -gtoggle -match=native -mtune=native
STAGE3_CFLAGS += -match=native -mtune=native
do-compare = $(SHELL) $(srcdir)/contrib/compare-debug $$f1 $$f2

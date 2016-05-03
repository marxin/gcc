# This option enables -fsanitize=address for stage2 and stage3.

# Suppress LeakSanitizer in bootstrap.
export LSAN_OPTIONS="detect_leaks=0"

STAGE1_CFLAGS += -O2
# STAGE2_CFLAGS += -fsanitize=address -fsanitize=use-after-scope -fstack-reuse=none
STAGE3_CFLAGS += -fsanitize=address -fsanitize=use-after-scope -fstack-reuse=none
STAGE3_LDFLAGS += -fsanitize=address -fsanitize=use-after-scope -static-libasan \
		      -B$$r/prev-$(TARGET_SUBDIR)/libsanitizer/ \
		      -B$$r/prev-$(TARGET_SUBDIR)/libsanitizer/asan/ \
		      -B$$r/prev-$(TARGET_SUBDIR)/libsanitizer/asan/.libs

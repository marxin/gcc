# This option enables -fsanitize=address for stage2 and stage3.

# Suppress LeakSanitizer in bootstrap.
export ASAN_OPTIONS=detect_leaks=0:use_odr_indicator=1:detect_invalid_pointer_pairs=1:halt_on_error=0:log_path=/tmp/gcc-asan-logs

STAGE2_CFLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract
STAGE3_CFLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract
POSTSTAGE1_LDFLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -static-libasan \
		      -B$$r/prev-$(TARGET_SUBDIR)/libsanitizer/ \
		      -B$$r/prev-$(TARGET_SUBDIR)/libsanitizer/asan/ \
		      -B$$r/prev-$(TARGET_SUBDIR)/libsanitizer/asan/.libs


LIBMFX_TOP_DIR := $(LIBMFX_PREBUILTS_DIR)/$(BUILD_TARGET_TAG)

$(call define-emulator-prebuilt-library,\
    emulator-libmfx,\
    $(LIBMFX_TOP_DIR)/lib/libmfx.a)

LIBMFX_INCLUDES := $(LIBMFX_TOP_DIR)/include
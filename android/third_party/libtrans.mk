$(call define-emulator-prebuilt-library,\
    emulator-libtrans,\
    $(LIBTRANS_PREBUILTS_DIR)/$(BUILD_TARGET_TAG)/lib/libtrans.a)

LIBTRANS_INCLUDES := $(LIBTRANS_PREBUILTS_DIR)/$(BUILD_TARGET_TAG)/include

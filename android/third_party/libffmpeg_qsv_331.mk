# Build rules for the static ffmpeg prebuilt libraries.
FFMPEG_QSV_331_TOP_DIR := $(FFMPEG_QSV_331_PREBUILTS_DIR)/$(BUILD_TARGET_TAG)

$(call define-emulator-prebuilt-library,\
    emulator-libavcodec-qsv331,\
    $(FFMPEG_QSV_331_TOP_DIR)/lib/libavcodec.a)

$(call define-emulator-prebuilt-library,\
    emulator-libavdevice-qsv331,\
    $(FFMPEG_QSV_331_TOP_DIR)/lib/libavdevice.a)

$(call define-emulator-prebuilt-library,\
    emulator-libavfilter-qsv331,\
    $(FFMPEG_QSV_331_TOP_DIR)/lib/libavfilter.a)

$(call define-emulator-prebuilt-library,\
    emulator-libavformat-qsv331,\
    $(FFMPEG_QSV_331_TOP_DIR)/lib/libavformat.a)

$(call define-emulator-prebuilt-library,\
    emulator-libavutil-qsv331,\
    $(FFMPEG_QSV_331_TOP_DIR)/lib/libavutil.a)

$(call define-emulator-prebuilt-library,\
    emulator-libswscale-qsv331,\
    $(FFMPEG_QSV_331_TOP_DIR)/lib/libswscale.a)

$(call define-emulator-prebuilt-library,\
    emulator-libswresample-qsv331,\
    $(FFMPEG_QSV_331_TOP_DIR)/lib/libswresample.a)

FFMPEG_QSV_331_INCLUDES := $(FFMPEG_QSV_331_TOP_DIR)/include
FFMPEG_QSV_331_STATIC_LIBRARIES := \
    emulator-libavdevice-qsv331 \
    emulator-libavformat-qsv331 \
    emulator-libavfilter-qsv331 \
    emulator-libavcodec-qsv331 \
    emulator-libswresample-qsv331 \
    emulator-libswscale-qsv331 \
    emulator-libavutil-qsv331 \

FFMPEG_QSV_331_LDLIBS := -L$(FFMPEG_QSV_331_TOP_DIR)/lib -lva -lva-drm -lva-x11

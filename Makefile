PROJECT_NAME := BOOTLOADER_2023
ifeq ($(ADF_PATH),)
  $(error Please specify ADF_PATH)
endif

include $(ADF_PATH)/project.mk

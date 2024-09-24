
# PLC_H743

set(PLC_H743_VARIANT_PATH "${CMAKE_CURRENT_LIST_DIR}/../variants/STM32H7xx/H743")
set(PLC_H743_MAXSIZE 2097152)
set(PLC_H743_MAXDATASIZE 524288)
set(PLC_H743_MCU cortex-m7)
set(PLC_H743_FPCONF "-")
add_library(PLC_H743 INTERFACE)
target_compile_options(PLC_H743 INTERFACE
  "SHELL:-DCORE_CM7 -DSTM32H743xx  "
  "SHELL:"
  "SHELL:"
  "SHELL:-mfpu=fpv4-sp-d16 -mfloat-abi=hard"
  -mcpu=${PLC_H743_MCU}
)
target_compile_definitions(PLC_H743 INTERFACE
  "STM32H7xx"
	"ARDUINO_PLC_H743"W
	"BOARD_NAME=\"PLC_H743\""
	"BOARD_ID=PLC_H743"
	"VARIANT_H=\"variant_PLC_H743.h\""
)
target_include_directories(PLC_H743 INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/../system/STM32H7xx
  ${CMAKE_CURRENT_LIST_DIR}/../system/Drivers/STM32H7xx_HAL_Driver/Inc
  ${CMAKE_CURRENT_LIST_DIR}/../system/Drivers/STM32H7xx_HAL_Driver/Src
  ${CMAKE_CURRENT_LIST_DIR}/../system/Drivers/CMSIS/Device/ST/STM32H7xx/Include/
  ${CMAKE_CURRENT_LIST_DIR}/../system/Drivers/CMSIS/Device/ST/STM32H7xx/Source/Templates/gcc/
  ${PLC_H743_VARIANT_PATH}
)

target_link_options(PLC_H743 INTERFACE
  "LINKER:--default-script=${PLC_H743_VARIANT_PATH}/ldscript.ld"
  "LINKER:--defsym=LD_FLASH_OFFSET=0"
	"LINKER:--defsym=LD_MAX_SIZE=2097152"
	"LINKER:--defsym=LD_MAX_DATA_SIZE=524288"
  "SHELL:-mfpu=fpv4-sp-d16 -mfloat-abi=hard"
  -mcpu=${PLC_H743_MCU}
)
target_link_libraries(PLC_H743 INTERFACE
  arm_cortexM7lfsp_math
)

add_library(PLC_H743_serial_disabled INTERFACE)
target_compile_options(PLC_H743_serial_disabled INTERFACE
  "SHELL:"
)
add_library(PLC_H743_serial_PLC INTERFACE)
target_compile_options(PLC_H743_serial_generic INTERFACE
  "SHELL:-DHAL_UART_MODULE_ENABLED"
)
add_library(PLC_H743_serial_none INTERFACE)
target_compile_options(PLC_H743_serial_none INTERFACE
  "SHELL:-DHAL_UART_MODULE_ENABLED -DHWSERIAL_NONE"
)
add_library(PLC_H743_usb_CDC INTERFACE)
target_compile_options(PLC_H743_usb_CDC INTERFACE
  "SHELL:-DUSBCON  -DUSBD_VID=0 -DUSBD_PID=0 -DHAL_PCD_MODULE_ENABLED -DUSBD_USE_CDC -DDISABLE_GENERIC_SERIALUSB"
)
add_library(PLC_H743_usb_CDCgen INTERFACE)
target_compile_options(PLC_H743_usb_CDCgen INTERFACE
  "SHELL:-DUSBCON  -DUSBD_VID=0 -DUSBD_PID=0 -DHAL_PCD_MODULE_ENABLED -DUSBD_USE_CDC"
)
add_library(PLC_H743_usb_HID INTERFACE)
target_compile_options(PLC_H743_usb_HID INTERFACE
  "SHELL:-DUSBCON  -DUSBD_VID=0 -DUSBD_PID=0 -DHAL_PCD_MODULE_ENABLED -DUSBD_USE_HID_COMPOSITE"
)
add_library(PLC_H743_usb_none INTERFACE)
target_compile_options(PLC_H743_usb_none INTERFACE
  "SHELL:"
)
add_library(PLC_H743_xusb_FS INTERFACE)
target_compile_options(PLC_H743_xusb_FS INTERFACE
  "SHELL:"
)
add_library(PLC_H743_xusb_HS INTERFACE)
target_compile_options(PLC_H743_xusb_HS INTERFACE
  "SHELL:-DUSE_USB_HS"
)
add_library(PLC_H743_xusb_HSFS INTERFACE)
target_compile_options(PLC_H743_xusb_HSFS INTERFACE
  "SHELL:-DUSE_USB_HS -DUSE_USB_HS_IN_FS"
)

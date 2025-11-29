
# Consider dependencies only in project.
set(CMAKE_DEPENDS_IN_PROJECT_ONLY OFF)

# The set of languages for which implicit dependencies are needed:
set(CMAKE_DEPENDS_LANGUAGES
  "ASM"
  )
# The set of files for implicit dependencies of each language:
set(CMAKE_DEPENDS_CHECK_ASM
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/portable/GCC/RISC-V/common/portASM.S" "D:/AiPi-Open-Kits2/AiPi-Voice-Assistant-V2/build/build_out/components/os/freertos/CMakeFiles/freertos.dir/portable/GCC/RISC-V/common/portASM.S.obj"
  )
set(CMAKE_ASM_COMPILER_ID "GNU")

# Preprocessor definitions for this target.
set(CMAKE_TARGET_DEFINITIONS_ASM
  "ARCH_RISCV"
  "BFLB_TIMESTAMP_TIMEZONE=8"
  "BFLB_USE_HAL_DRIVER"
  "BFLB_USE_ROM_DRIVER"
  "BL616"
  "BOUFFALO_SDK"
  "CONFIG_EASYFLASH4"
  "CONFIG_FREERTOS"
  "CONFIG_IRQ_NUM=80"
  "CONFIG_LOG_LEVEL=3"
  "CONFIG_LWIP"
  "CONFIG_MAC_RXQ_DEPTH=12"
  "CONFIG_MAC_TXQ_DEPTH=16"
  "CONFIG_POSIX"
  "CONFIG_PSRAM"
  "CONFIG_STA_MAX=4"
  "CONFIG_TLSF"
  "CONFIG_VIF_MAX=2"
  "MBEDTLS_CONFIG_FILE=\"mbedtls_sample_config.h\""
  "MBEDTLS_NET_C"
  "RFPARAM_BL616"
  "WL_BL616=1"
  "WL_BL618M=0"
  "WL_WB03=0"
  "configSTACK_ALLOCATION_FROM_SEPARATE_HEAP=1"
  "default_interrupt_handler=freertos_risc_v_trap_handler"
  "portasmHANDLE_INTERRUPT=interrupt_entry"
  )

# The include file search paths:
set(CMAKE_ASM_TARGET_INCLUDE_PATH
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/bsp/board/bl616dk/."
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/crypto/mbedtls/."
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/crypto/mbedtls/mbedtls/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/crypto/mbedtls/port"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/crypto/mbedtls/port/platform"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/libc/newlib/."
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/libc/."
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/mm/."
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/mm/tlsf/."
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/net/lwip/lwip/system"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/net/lwip/lwip/src/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/net/lwip/lwip/src/include/lwip/apps"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/net/lwip/lwip/src/include/compat/posix"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/net/lwip/lwip_apps/dhcpd/."
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/portable/GCC/RISC-V/common"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/portable/GCC/RISC-V/common/chip_specific_extensions/RV32I_CLINT_no_extensions"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/utils/log"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/utils/ring_buffer"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/utils/bflb_block_pool"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/utils/bflb_timestamp"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/utils/getopt"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/utils/partition"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/utils/bflb_mtd/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/wireless/wifi6/inc"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/wireless/wifi6/bl6_os_adapter/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/easyflash4/inc"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/lhal/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/lhal/include/arch"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/lhal/include/arch/risc-v/t-head"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/lhal/include/arch/risc-v/t-head/Core/Include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/lhal/include/arch/risc-v/t-head/csi_dsp/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/lhal/config/bl616"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/lhal/src/flash"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/include"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/include/hardware"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/rf/inc"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/rfparam/Inc"
  "D:/AiPi-Open-Kits2/AiPi-Voice-Assistant-V2/."
  )

# The set of dependency files which are needed:
set(CMAKE_DEPENDS_DEPENDENCY_FILES
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/croutine.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/croutine.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/croutine.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/event_groups.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/event_groups.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/event_groups.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/freertos_port.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/freertos_port.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/freertos_port.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/list.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/list.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/list.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/portable/GCC/RISC-V/common/port.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/portable/GCC/RISC-V/common/port.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/portable/GCC/RISC-V/common/port.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/portable/MemMang/heap_3.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/portable/MemMang/heap_3.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/portable/MemMang/heap_3.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_clock.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_clock.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_clock.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_mqueue.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_mqueue.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_mqueue.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_pthread.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_pthread.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_pthread.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_pthread_barrier.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_pthread_barrier.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_pthread_barrier.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_pthread_cond.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_pthread_cond.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_pthread_cond.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_pthread_mutex.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_pthread_mutex.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_pthread_mutex.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_sched.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_sched.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_sched.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_semaphore.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_semaphore.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_semaphore.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_timer.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_timer.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_timer.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_unistd.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_unistd.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_unistd.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/posix/source/FreeRTOS_POSIX_utils.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_utils.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/posix/source/FreeRTOS_POSIX_utils.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/queue.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/queue.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/queue.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/tasks.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/tasks.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/tasks.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/components/os/freertos/timers.c" "build_out/components/os/freertos/CMakeFiles/freertos.dir/timers.c.obj" "gcc" "build_out/components/os/freertos/CMakeFiles/freertos.dir/timers.c.obj.d"
  )

# Targets to which this target links.
set(CMAKE_TARGET_LINKED_INFO_FILES
  )

# Fortran module output directory.
set(CMAKE_Fortran_TARGET_MODULE_DIR "")

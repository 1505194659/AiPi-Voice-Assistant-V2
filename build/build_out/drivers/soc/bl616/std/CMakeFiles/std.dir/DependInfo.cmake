
# Consider dependencies only in project.
set(CMAKE_DEPENDS_IN_PROJECT_ONLY OFF)

# The set of languages for which implicit dependencies are needed:
set(CMAKE_DEPENDS_LANGUAGES
  "ASM"
  )
# The set of files for implicit dependencies of each language:
set(CMAKE_DEPENDS_CHECK_ASM
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/startup/riscv_fpu.S" "D:/AiPi-Open-Kits2/AiPi-Voice-Assistant-V2/build/build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/riscv_fpu.S.obj"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/startup/start.S" "D:/AiPi-Open-Kits2/AiPi-Voice-Assistant-V2/build/build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/start.S.obj"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/startup/vector.S" "D:/AiPi-Open-Kits2/AiPi-Voice-Assistant-V2/build/build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/vector.S.obj"
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
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/port/bl616_clock.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/port/bl616_clock.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/port/bl616_clock.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_aon.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_aon.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_aon.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_common.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_common.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_common.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_ef_cfg.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_ef_cfg.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_ef_cfg.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_mfg_efuse.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_mfg_efuse.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_mfg_efuse.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_mfg_flash.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_mfg_flash.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_mfg_flash.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_mfg_media.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_mfg_media.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_mfg_media.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_pm.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_pm.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_pm.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_psram.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_psram.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_psram.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_romapi_e907.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_romapi_e907.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_romapi_e907.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_romapi_patch.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_romapi_patch.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_romapi_patch.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_sdh.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_sdh.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_sdh.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/src/bl616_tzc_sec.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_tzc_sec.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/src/bl616_tzc_sec.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/startup/interrupt.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/interrupt.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/interrupt.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/startup/start_load.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/start_load.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/start_load.c.obj.d"
  "D:/AiPi-Open-Kits2/aithinker_Ai-M6X_SDK/drivers/soc/bl616/std/startup/system_bl616.c" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/system_bl616.c.obj" "gcc" "build_out/drivers/soc/bl616/std/CMakeFiles/std.dir/startup/system_bl616.c.obj.d"
  )

# Targets to which this target links.
set(CMAKE_TARGET_LINKED_INFO_FILES
  )

# Fortran module output directory.
set(CMAKE_Fortran_TARGET_MODULE_DIR "")

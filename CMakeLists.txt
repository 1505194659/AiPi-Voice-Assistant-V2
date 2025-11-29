cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

target_sources(app PRIVATE
    bsp_es8388.c
    https_client.c
    cJSON.c
    whisper_live_client.c
    stt_client.c
    deepseek_client.c
    tts_client.c
    vad.c
)

sdk_add_include_directories(.)

sdk_set_main_file(main.c)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${PROJECT_NAME})

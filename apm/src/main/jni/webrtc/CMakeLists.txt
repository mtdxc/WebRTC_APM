cmake_minimum_required(VERSION 3.15)

project(webrtc-audio-processing)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    add_definitions(-DWEBRTC_MAC)
    set(have_posix TRUE)
    set(is_mac TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "iOS")
    add_definitions(-DWEBRTC_IOS)
    set(have_posix TRUE)
    set(is_ios TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
    add_definitions(-DWEBRTC_ANDROID -DWEBRTC_LINUX)
    set(have_posix TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWEBRTC_LINUX -DWEBRTC_THREAD_RR")
    add_definitions(-DWEBRTC_LINUX -DWEBRTC_THREAD_RR)
    set(have_posix TRUE)
else()
    add_definitions(-DWEBRTC_WIN -DNOMINMAX)
endif()

if(have_posix)
    message(STATUS "have_posix ${have_posix}")
    add_definitions(-DWEBRTC_POSIX)
endif()


include(CheckCXXSourceCompiles)
foreach(key
  IN ITEMS
    NUMBER_OF_LOGICAL_CORES
    NUMBER_OF_PHYSICAL_CORES
    TOTAL_VIRTUAL_MEMORY
    AVAILABLE_VIRTUAL_MEMORY
    TOTAL_PHYSICAL_MEMORY
    AVAILABLE_PHYSICAL_MEMORY
    IS_64BIT
    HAS_FPU
    HAS_MMX
    HAS_MMX_PLUS
    HAS_SSE
    HAS_SSE2
    HAS_SSE_FP
    HAS_SSE_MMX
    HAS_AMD_3DNOW
    HAS_AMD_3DNOW_PLUS
    HAS_IA64
    OS_NAME
    OS_RELEASE
    OS_VERSION
    OS_PLATFORM
  )
  cmake_host_system_information(RESULT _${key} QUERY ${key})
endforeach()

check_cxx_source_compiles("
    #ifndef __ARM_ARCH_ISA_ARM
    #error no arm arch
    #endif
    int main(){return 0;}
    " have_arm)

check_cxx_source_compiles("
    #ifndef __ARM_ARCH_7A__
    #error no armv7 arch
    #endif
    int main(){return 0;}
    " have_armv7)

check_cxx_source_compiles("
    #ifndef __aarch64__
    #error no aarch64 arch
    #endif
    int main(){return 0;}
    " have_arm64)

message(STATUS "have_arm: ${have_arm}")
message(STATUS "have_armv7: ${have_armv7}")
message(STATUS "have_arm64: ${have_arm64}")

if (have_arm)
    add_definitions(-DWEBRTC_ARCH_ARM)
endif ()
if(have_armv7)
    add_definitions(-DWEBRTC_ARCH_ARM_V7)
endif()
if(have_arm64)
    add_definitions(-DWEBRTC_ARCH_ARM64 -DWEBRTC_HAS_NEON)
    set(have_neon TRUE)
endif()
message(STATUS "${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(x86_64)|(i686)")
    add_definitions(-DWEBRTC_ENABLE_AVX2)
    set(have_avx2 TRUE)
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(mips)|(mips64)")
    add_definitions(-DWEBRTC_ARCH_MIPS_FAMILY)
    set(have_mips TRUE)
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(mips64)")
    set(have_mips64 TRUE)
endif()

if (have_neon)
    add_definitions(-DWEBRTC_HAS_NEON)
endif()

include_directories(.)
set(apm_src
    "webrtc/common_audio/fft4g.c"
    "webrtc/common_audio/ring_buffer.c"
    "webrtc/common_audio/signal_processing/complex_bit_reverse.c"
    "webrtc/common_audio/signal_processing/complex_fft.c"
    "webrtc/common_audio/signal_processing/copy_set_operations.c"
    "webrtc/common_audio/signal_processing/cross_correlation.c"
    "webrtc/common_audio/signal_processing/division_operations.c"
    "webrtc/common_audio/signal_processing/downsample_fast.c"
    "webrtc/common_audio/signal_processing/energy.c"
    "webrtc/common_audio/signal_processing/get_scaling_square.c" 
    "webrtc/common_audio/signal_processing/min_max_operations.c" 
    "webrtc/common_audio/signal_processing/randomization_functions.c"
    "webrtc/common_audio/signal_processing/real_fft.c" 
    "webrtc/common_audio/signal_processing/spl_init.c" 
    "webrtc/common_audio/signal_processing/spl_sqrt.c" 
    "webrtc/common_audio/signal_processing/spl_sqrt_floor.c" 
    "webrtc/common_audio/signal_processing/vector_scaling_operations.c" 
    "webrtc/common_audio/signal_processing/resample_48khz.c" 
    "webrtc/common_audio/signal_processing/resample_by_2_internal.c" 
    "webrtc/common_audio/signal_processing/resample_by_2.c" 
    "webrtc/common_audio/signal_processing/resample_fractional.c" 
    "webrtc/common_audio/signal_processing/dot_product_with_scale.c" 
    "webrtc/common_audio/vad/vad_core.c" 
    "webrtc/common_audio/vad/vad_filterbank.c" 
    "webrtc/common_audio/vad/vad_gmm.c" 
    "webrtc/common_audio/vad/vad_sp.c" 
    "webrtc/common_audio/vad/webrtc_vad.c"
    "webrtc/modules/audio_processing/aecm/aecm_core_c.c"
    "webrtc/modules/audio_processing/ns/nsx_core_c.c"
    "webrtc/modules/audio_processing/aec/aec_core.c" 
    "webrtc/modules/audio_processing/aec/aec_rdft.c" 
    "webrtc/modules/audio_processing/aec/aec_resampler.c"
    "webrtc/modules/audio_processing/aec/echo_cancellation.c"
    "webrtc/modules/audio_processing/aecm/aecm_core.c" 
    "webrtc/modules/audio_processing/aecm/echo_control_mobile.c" 
    "webrtc/modules/audio_processing/ns/noise_suppression.c" 
    "webrtc/modules/audio_processing/ns/noise_suppression_x.c" 
    "webrtc/modules/audio_processing/ns/ns_core.c" 
    "webrtc/modules/audio_processing/ns/nsx_core.c"
    "webrtc/modules/audio_processing/agc/digital_agc.c" 
    "webrtc/modules/audio_processing/agc/analog_agc.c"
    "webrtc/modules/audio_processing/utility/delay_estimator_wrapper.c" 
    "webrtc/modules/audio_processing/utility/delay_estimator.c"
    )
if (have_neon)
  list(APPEND apm_src 
    "webrtc/common_audio/signal_processing/cross_correlation_neon.c" 
    "webrtc/common_audio/signal_processing/downsample_fast_neon.c" 
    "webrtc/common_audio/signal_processing/min_max_operations_neon.c"
    "webrtc/modules/audio_processing/aecm/aecm_core_neon.c"
    "webrtc/modules/audio_processing/aec/aec_core_neon.c"
    "webrtc/modules/audio_processing/aec/aec_rdft_neon.c"
    "webrtc/modules/audio_processing/ns/nsx_core_neon.c" 
    )
endif()

if(have_mips)
  list(APPEND apm_src 
    "webrtc/common_audio/signal_processing/cross_correlation_mips.c"
    "webrtc/common_audio/signal_processing/downsample_fast_mips.c"
    "webrtc/common_audio/signal_processing/min_max_operations_mpis.c"
    "webrtc/modules/audio_processing/aecm/aecm_core_mips.c"
    "webrtc/modules/audio_processing/aec/aec_core_mips.c"
    "webrtc/modules/audio_processing/aec/aec_rdft_mips.c"
    "webrtc/modules/audio_processing/ns/nsx_core_mips.c" 
    )
endif()
if(_HAS_SSE)
  list(APPEND apm_src 
    "webrtc/modules/audio_processing/aec/aec_core_sse2.c"
    "webrtc/modules/audio_processing/aec/aec_rdft_sse2.c"
    "webrtc/system_wrappers/source/cpu_features.cc"
    )
endif()

file(GLOB AECM_SRC
${CMAKE_CURRENT_LIST_DIR}/aecm/*.c
${CMAKE_CURRENT_LIST_DIR}/aecm/*.cc
)
#list(FILTER AECM_SRC EXCLUDE REGEX ".*aecm_core_c.cc$")
list(FILTER AECM_SRC EXCLUDE REGEX ".*aecm_core_neon.cc$")
list(FILTER AECM_SRC EXCLUDE REGEX ".*aecm_core_mips.cc$")
set(AECM_COMPILE_CODE ${AECM_SRC})

add_library(rtc_apm SHARED ${apm_src})
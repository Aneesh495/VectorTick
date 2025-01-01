# Architecture detection and configuration

function(detect_cpu_features)
    # Try to detect CPU features
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
        try_compile(HAS_AVX2
            ${CMAKE_BINARY_DIR}/detect_avx2
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/detect_avx2.cpp
            COMPILE_DEFINITIONS "-mavx2"
        )
        if(HAS_AVX2)
            set(VT_HAS_AVX2 ON CACHE INTERNAL "CPU supports AVX2")
        endif()
        
        try_compile(HAS_SSE42
            ${CMAKE_BINARY_DIR}/detect_sse42
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/detect_sse42.cpp
            COMPILE_DEFINITIONS "-msse4.2"
        )
        if(HAS_SSE42)
            set(VT_HAS_SSE42 ON CACHE INTERNAL "CPU supports SSE4.2")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(VT_HAS_NEON ON CACHE INTERNAL "ARM supports NEON")
    endif()
endfunction()

# Detect at configure time
detect_cpu_features()


set(GHEX_USE_GPU "OFF" CACHE BOOL "use gpu")
set(GHEX_EMULATE_GPU "OFF" CACHE BOOL "emulate gpu (for debugging)")
#mark_as_advanced(GHEX_EMULATE_GPU)

if(GHEX_USE_GPU)
    if (NOT ${HWMALLOC_ENABLE_DEVICE})
        message(FATAL_ERROR "GPU backend is not available - check hwmalloc configure options")
    endif()
    set(GHEX_GPU_TYPE "AUTO" CACHE STRING "Choose the GPU type: AMD | NVIDIA | AUTO (environment-based)")
    # Set the possible values of GPU type for cmake-gui
    set_property(CACHE GHEX_GPU_TYPE PROPERTY STRINGS "AMD" "NVIDIA" "AUTO")
    if (GHEX_GPU_TYPE STREQUAL "AMD")
        if (NOT ${HWMALLOC_DEVICE_RUNTIME} STREQUAL "hip")
            message(FATAL_ERROR "AMD/hip backend is not available - check hwmalloc configure options")
        endif()
        find_package(hip REQUIRED)
        set(ghex_gpu_mode "hip")
    elseif (GHEX_GPU_TYPE STREQUAL "NVIDIA")
        if (NOT ${HWMALLOC_DEVICE_RUNTIME} STREQUAL "cuda")
            message(FATAL_ERROR "Cuda backend is not available - check hwmalloc configure options")
        endif()
        set(ghex_gpu_mode "cuda")
    elseif (GHEX_GPU_TYPE STREQUAL "AUTO")
        find_package(hip)
        if (hip_FOUND)
            if (NOT ${HWMALLOC_DEVICE_RUNTIME} STREQUAL "hip")
                message(FATAL_ERROR "AMD/hip backend is not available - check hwmalloc configure options")
            endif()
            set(ghex_gpu_mode "hip")
        else() # assume cuda elsewhere; TO DO: might be refined
            if (NOT ${HWMALLOC_DEVICE_RUNTIME} STREQUAL "cuda")
                message(FATAL_ERROR "Cuda backend is not available - check hwmalloc configure options")
            endif()
            set(ghex_gpu_mode "cuda")
        endif()
    endif()
    if (ghex_gpu_mode STREQUAL "cuda")
        set(CMAKE_CUDA_FLAGS "" CACHE STRING "")
        string(APPEND CMAKE_CUDA_FLAGS " --cudart shared --expt-relaxed-constexpr")
        enable_language(CUDA)
        set(CMAKE_CUDA_STANDARD 14)
        set(CMAKE_CUDA_EXTENSIONS OFF)
    endif()
    set(GHEX_GPU_MODE_EMULATE "OFF")
else()
    if (GHEX_EMULATE_GPU)
        if (NOT ${HWMALLOC_ENABLE_DEVICE})
            message(FATAL_ERROR "GPU backend is not available - check hwmalloc configure options")
        endif()
        if (NOT ${HWMALLOC_DEVICE_RUNTIME} STREQUAL "emulate")
            message(FATAL_ERROR "Emulate backend is not available - check hwmalloc configure options")
        endif()
        set(ghex_gpu_mode "emulate")
        set(GHEX_GPU_MODE_EMULATE "ON")
    else()
        if (${HWMALLOC_ENABLE_DEVICE})
            message(FATAL_ERROR "GPU backend is available - check hwmalloc configure options")
        endif()
        set(ghex_gpu_mode "none")
        set(GHEX_GPU_MODE_EMULATE "OFF")
    endif()
endif()


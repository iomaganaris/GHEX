
set(GHEX_USE_GPU "OFF" CACHE BOOL "use gpu")
set(GHEX_GPU_TYPE "AUTO" CACHE STRING "Choose the GPU type: AMD | NVIDIA | AUTO (environment-based) | EMULATE")
set_property(CACHE GHEX_GPU_TYPE PROPERTY STRINGS "AMD" "NVIDIA" "AUTO" "EMULATE")
set(GHEX_COMM_OBJ_USE_U "OFF" CACHE BOOL "uniform field optimization for gpu")

if (GHEX_USE_GPU)
    if (GHEX_GPU_TYPE STREQUAL "AUTO")
        find_package(hip)
        if (hip_FOUND)
            set(ghex_gpu_mode "hip")
        else() # assume cuda elsewhere; TO DO: might be refined
            set(ghex_gpu_mode "cuda")
        endif()
    elseif (GHEX_GPU_TYPE STREQUAL "AMD")
        set(ghex_gpu_mode "hip")
    elseif (GHEX_GPU_TYPE STREQUAL "NVIDIA")
        set(ghex_gpu_mode "cuda")
    else()
        set(ghex_gpu_mode "emulate")
    endif()

    if (ghex_gpu_mode STREQUAL "cuda")
        # set default cuda architecture
        if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
            set(CMAKE_CUDA_ARCHITECTURES 75)
        endif()
        set(CMAKE_CUDA_FLAGS "" CACHE STRING "")
        string(APPEND CMAKE_CUDA_FLAGS " --cudart shared --expt-relaxed-constexpr")
        enable_language(CUDA)
        set(CMAKE_CUDA_STANDARD 17)
        set(CMAKE_CUDA_EXTENSIONS OFF)
        set(GHEX_GPU_MODE_EMULATE "OFF")
    elseif (ghex_gpu_mode STREQUAL "hip")
        find_package(hip REQUIRED)
    else()
        set(GHEX_GPU_MODE_EMULATE "ON")
    endif()
else()
    set(ghex_gpu_mode "none")
    set(GHEX_GPU_MODE_EMULATE "OFF")
endif()

string(TOUPPER ${ghex_gpu_mode} ghex_gpu_mode_u)
set(GHEX_DEVICE "GHEX_DEVICE_${ghex_gpu_mode_u}")


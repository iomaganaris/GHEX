include(CMakeDependentOption)
include(git_submodule)
include(external_project)

if(GHEX_GIT_SUBMODULE)
    update_git_submodules()
endif()

# ---------------------------------------------------------------------
# MPI setup
# ---------------------------------------------------------------------
find_package(MPI REQUIRED)
target_link_libraries(ghex_common INTERFACE MPI::MPI_CXX)
target_link_libraries(ghex PRIVATE MPI::MPI_CXX)

# ---------------------------------------------------------------------
# Boost setup
# ---------------------------------------------------------------------
find_package(Boost REQUIRED)
target_link_libraries(ghex_common INTERFACE Boost::boost)

# ---------------------------------------------------------------------
# LibRt setup
# ---------------------------------------------------------------------
find_library(LIBRT rt REQUIRED)
target_link_libraries(ghex PUBLIC ${LIBRT})

# ---------------------------------------------------------------------
# GridTools setup
# ---------------------------------------------------------------------
cmake_dependent_option(GHEX_USE_BUNDLED_GRIDTOOLS "Use bundled gridtools." ON
    "GHEX_USE_BUNDLED_LIBS" OFF)
if(GHEX_USE_BUNDLED_GRIDTOOLS)
    check_git_submodule(GridTools ext/gridtools)
    add_subdirectory(ext/gridtools)
else()
    find_package(GridTools REQUIRED)
endif()
target_link_libraries(ghex_common INTERFACE GridTools::gridtools)

# ---------------------------------------------------------------------
# oomph setup
# ---------------------------------------------------------------------
set(GHEX_TRANSPORT_BACKEND "MPI" CACHE STRING "Choose the backend type: MPI | UCX | LIBFABRIC")
set_property(CACHE GHEX_TRANSPORT_BACKEND PROPERTY STRINGS "MPI" "UCX" "LIBFABRIC")
cmake_dependent_option(GHEX_USE_BUNDLED_OOMPH "Use bundled oomph." ON
    "GHEX_USE_BUNDLED_LIBS" OFF)
if(GHEX_USE_BUNDLED_OOMPH)
    check_git_submodule(oomph ext/oomph)
    set(OOMPH_USE_BUNDLED_LIBS ON CACHE BOOL "Use bundled 3rd party libraries" FORCE)
    if (GHEX_TRANSPORT_BACKEND STREQUAL "LIBFABRIC")
        set(OOMPH_WITH_MPI OFF CACHE BOOL "Build with MPI backend" FORCE)
        set(OOMPH_WITH_UCX OFF CACHE BOOL "Build with UCX backend" FORCE)
        set(OOMPH_WITH_LIBFABRIC ON CACHE BOOL "Build with LIBFABRIC backend" FORCE)
    elseif (GHEX_TRANSPORT_BACKEND STREQUAL "UCX")
        set(OOMPH_WITH_MPI OFF CACHE BOOL "Build with MPI backend" FORCE)
        set(OOMPH_WITH_UCX ON CACHE BOOL "Build with UCX backend" FORCE)
        set(OOMPH_WITH_LIBFABRIC OFF CACHE BOOL "Build with LIBFABRIC backend" FORCE)
    else()
        set(OOMPH_WITH_MPI ON CACHE BOOL "Build with MPI backend" FORCE)
        set(OOMPH_WITH_UCX OFF CACHE BOOL "Build with UCX backend" FORCE)
        set(OOMPH_WITH_LIBFABRIC OFF CACHE BOOL "Build with LIBFABRIC backend" FORCE)
    endif()
    if(GHEX_USE_GPU)
        set(HWMALLOC_ENABLE_DEVICE ON CACHE BOOL "True if GPU support shall be enabled" FORCE)
        if (GHEX_GPU_TYPE STREQUAL "NVIDIA")
            set(HWMALLOC_DEVICE_RUNTIME "cuda" CACHE STRING "Choose the type of the gpu runtime."
                FORCE)
        elseif (GHEX_GPU_TYPE STREQUAL "AMD")
            set(HWMALLOC_DEVICE_RUNTIME "hip" CACHE STRING "Choose the type of the gpu runtime."
                FORCE)
        endif()
    endif()
    add_subdirectory(ext/oomph)
else()
    find_package(oomph 0.2 REQUIRED)
endif()

target_link_libraries(ghex INTERFACE oomph::oomph)
target_link_libraries(ghex_common INTERFACE oomph::oomph)
function(ghex_link_to_oomph target)
    if (GHEX_TRANSPORT_BACKEND STREQUAL "LIBFABRIC")
        target_link_libraries(${target} PRIVATE oomph_libfabric)
    elseif (GHEX_TRANSPORT_BACKEND STREQUAL "UCX")
        target_link_libraries(${target} PRIVATE oomph_ucx)
    else()
        target_link_libraries(${target} PRIVATE oomph_mpi)
    endif()
endfunction()

# ---------------------------------------------------------------------
# general RMA setup
# ---------------------------------------------------------------------
set(GHEX_NO_RMA OFF CACHE BOOL "Disable in-node RMA completely")

# ---------------------------------------------------------------------
# xpmem setup
# ---------------------------------------------------------------------
set(GHEX_USE_XPMEM OFF CACHE BOOL "Set to true to use xpmem shared memory")
if (GHEX_USE_XPMEM)
    find_package(XPMEM REQUIRED)
    target_link_libraries(ghex_common INTERFACE XPMEM::libxpmem)
    target_link_libraries(ghex PRIVATE XPMEM::libxpmem)
endif()
set(GHEX_USE_XPMEM_ACCESS_GUARD OFF CACHE BOOL "Use xpmem to synchronize rma access")
mark_as_advanced(GHEX_USE_XPMEM_ACCESS_GUARD)

# ---------------------------------------------------------------------
# parmetis setup
# ---------------------------------------------------------------------
set(GHEX_ENABLE_PARMETIS_BINDINGS OFF CACHE BOOL "Set to true to build with ParMETIS bindings")
if (GHEX_ENABLE_PARMETIS_BINDINGS)
    set(METIS_INCLUDE_DIR "" CACHE STRING "METIS include directory")
    set(METIS_LIB_DIR "" CACHE STRING "METIS library directory")
    set(PARMETIS_INCLUDE_DIR "" CACHE STRING "ParMETIS include directory")
    set(PARMETIS_LIB_DIR "" CACHE STRING "ParMETIS library directory")
endif()

# ---------------------------------------------------------------------
# atlas setup
# ---------------------------------------------------------------------
set(GHEX_ENABLE_ATLAS_BINDINGS OFF CACHE BOOL "Set to true to build with Atlas bindings")
if (GHEX_ENABLE_ATLAS_BINDINGS)
    find_package(eckit REQUIRED HINTS ${eckit_DIR})
    find_package(Atlas REQUIRED HINTS ${Atlas_DIR})
    set(GHEX_ATLAS_GT_STORAGE_CPU_BACKEND "KFIRST" CACHE STRING "GridTools CPU storage traits: KFIRST | IFIRST.")
    set_property(CACHE GHEX_ATLAS_GT_STORAGE_CPU_BACKEND PROPERTY STRINGS "KFIRST" "IFIRST")
    # Temporary workaround to fix missing dependency in Atlas target: eckit
    target_link_libraries(atlas INTERFACE eckit)
    target_link_libraries(ghex_common INTERFACE atlas)

    if (GHEX_ATLAS_GT_STORAGE_CPU_BACKEND STREQUAL "KFIRST")
        set(GHEX_ATLAS_GT_STORAGE_CPU_BACKEND_KFIRST ON)
        set(GHEX_ATLAS_GT_STORAGE_CPU_BACKEND_IFIRST OFF)
    elseif(GHEX_ATLAS_GT_STORAGE_CPU_BACKEND STREQUAL "IFIRST")
        set(GHEX_ATLAS_GT_STORAGE_CPU_BACKEND_KFIRST OFF)
        set(GHEX_ATLAS_GT_STORAGE_CPU_BACKEND_IFIRST ON)
    else()
        set(GHEX_ATLAS_GT_STORAGE_CPU_BACKEND_KFIRST OFF)
        set(GHEX_ATLAS_GT_STORAGE_CPU_BACKEND_IFIRST OFF)
    endif()
endif()

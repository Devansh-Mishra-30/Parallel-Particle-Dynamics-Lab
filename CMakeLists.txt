cmake_minimum_required(VERSION 3.16)
project(particle_simulator_v2 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler options for high performance and rigorous diagnostics
add_library(sim_v2_compile_flags INTERFACE)
target_compile_options(sim_v2_compile_flags INTERFACE
  -Wall
  -Wextra
  -Wpedantic
  $<$<CONFIG:Release>:-O3 -march=native>
  $<$<CONFIG:Debug>:-g -O0>
)

# -----------------------------------------------------------------------------
# sim_core Library (Physics & Dynamics Foundation)
# -----------------------------------------------------------------------------
set(SIM_CORE_SOURCES
  sim_core/src/contact.cpp
  sim_core/src/boundary.cpp
  sim_core/src/diagnostics.cpp
  sim_core/src/cpu_reference_solver.cpp
)

add_library(sim_core STATIC ${SIM_CORE_SOURCES})
target_include_directories(sim_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/sim_core/include
)
target_link_libraries(sim_core PRIVATE sim_v2_compile_flags)

# -----------------------------------------------------------------------------
# sim_experiments Library (Educational Experiment Framework)
# -----------------------------------------------------------------------------
set(SIM_EXPERIMENTS_SOURCES
  experiments/src/collision_experiment.cpp
  experiments/src/gravity_experiment.cpp
  experiments/src/convergence_experiment.cpp
)

add_library(sim_experiments STATIC ${SIM_EXPERIMENTS_SOURCES})
target_include_directories(sim_experiments PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/experiments/include
)
target_link_libraries(sim_experiments PUBLIC sim_core PRIVATE sim_v2_compile_flags)

# -----------------------------------------------------------------------------
# sim_omp_v2 Library (High-Performance Multicore OpenMP Backend)
# -----------------------------------------------------------------------------
find_package(OpenMP REQUIRED)

set(SIM_OMP_SOURCES
  sim_omp_v2/src/omp_solver.cpp
  sim_omp_v2/src/spatial_grid.cpp
)

add_library(sim_omp_v2 STATIC ${SIM_OMP_SOURCES})
target_include_directories(sim_omp_v2 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/sim_omp_v2/include
  ${CMAKE_CURRENT_SOURCE_DIR}/sim_core/include
)
target_link_libraries(sim_omp_v2 PUBLIC sim_core OpenMP::OpenMP_CXX PRIVATE sim_v2_compile_flags)

# -----------------------------------------------------------------------------
# sim_cuda_v2 Library (High-Performance GPU Accelerated Backend - Phase 3)
# -----------------------------------------------------------------------------
option(ENABLE_CUDA "Enable NVIDIA CUDA GPU Acceleration" ON)

if(ENABLE_CUDA)
  include(CheckLanguage)
  check_language(CUDA)

  if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    set(CMAKE_CUDA_ARCHITECTURES 89 CACHE STRING "CUDA compute architecture" FORCE)
    set(CUDA_BACKEND_AVAILABLE ON)

    set(SIM_CUDA_SOURCES
      sim_cuda_v2/src/cuda_solver.cu
      sim_cuda_v2/src/cuda_spatial_grid.cu
    )

    add_library(sim_cuda_v2 STATIC ${SIM_CUDA_SOURCES})
    target_include_directories(sim_cuda_v2 PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/sim_cuda_v2/include
      ${CMAKE_CURRENT_SOURCE_DIR}/sim_core/include
    )
    target_link_libraries(sim_cuda_v2 PUBLIC sim_core)
    target_compile_definitions(sim_cuda_v2 PUBLIC HAS_CUDA_BACKEND=1)
    add_compile_definitions(HAS_CUDA_BACKEND=1)
    message(STATUS "NVIDIA CUDA backend enabled (Compute Architecture: sm_89).")
  else()
    message(STATUS "CUDA compiler not found. Building without CUDA GPU backend.")
    set(CUDA_BACKEND_AVAILABLE OFF)
  endif()
else()
  message(STATUS "CUDA backend disabled by configuration (ENABLE_CUDA=OFF).")
  set(CUDA_BACKEND_AVAILABLE OFF)
endif()

# -----------------------------------------------------------------------------
# Third-Party Dependencies for Interactive GUI (GLFW, Dear ImGui, ImPlot)
# -----------------------------------------------------------------------------
option(BUILD_LAB_GUI "Build the interactive scientific desktop GUI (Phase 1B)" ON)

if(BUILD_LAB_GUI)
  find_package(OpenGL REQUIRED)

  include(FetchContent)

  # 1. GLFW 3.3.8
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
    GIT_SHALLOW TRUE
  )

  # 2. Dear ImGui v1.90.4-docking
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4-docking
    GIT_SHALLOW TRUE
  )

  # 3. ImPlot v0.16
  FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.16
    GIT_SHALLOW TRUE
  )

  FetchContent_MakeAvailable(glfw imgui implot)

  # Dear ImGui library target with GLFW + OpenGL3 backends
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  target_compile_definitions(imgui PUBLIC GL_GLEXT_PROTOTYPES)
  target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

  # ImPlot library target
  add_library(implot STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp
  )
  target_include_directories(implot PUBLIC
    ${implot_SOURCE_DIR}
  )
  target_link_libraries(implot PUBLIC imgui)

  # -----------------------------------------------------------------------------
  # Interactive Lab GUI Executable (sim_lab)
  # -----------------------------------------------------------------------------
  set(LAB_GUI_SOURCES
    lab_gui/src/viewport.cpp
    lab_gui/src/controls_panel.cpp
    lab_gui/src/diagnostics_panel.cpp
    lab_gui/src/screenshot_util.cpp
    lab_gui/src/app.cpp
    lab_gui/src/main.cpp
  )

  add_executable(sim_lab ${LAB_GUI_SOURCES})
  set(LAB_GUI_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/lab_gui/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_omp_v2/include
    ${glfw_SOURCE_DIR}/deps
  )
  if(CUDA_BACKEND_AVAILABLE)
    list(APPEND LAB_GUI_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/sim_cuda_v2/include)
  endif()
  target_include_directories(sim_lab PUBLIC ${LAB_GUI_INCLUDES})

  set(LAB_GUI_LIBS
    sim_core
    sim_experiments
    sim_omp_v2
    imgui
    implot
    glfw
    OpenGL::GL
    sim_v2_compile_flags
  )
  if(CUDA_BACKEND_AVAILABLE)
    list(APPEND LAB_GUI_LIBS sim_cuda_v2)
  endif()
  target_link_libraries(sim_lab PRIVATE ${LAB_GUI_LIBS})
endif()

# -----------------------------------------------------------------------------
# Automated Physics Validation & Experiment Regression Test Suites
# -----------------------------------------------------------------------------
enable_testing()

add_executable(test_physics tests/test_physics.cpp)
target_link_libraries(test_physics PRIVATE sim_core sim_v2_compile_flags)
add_test(NAME PhysicsVerification COMMAND test_physics)

add_executable(test_experiments tests/test_experiments.cpp)
target_link_libraries(test_experiments PRIVATE sim_experiments sim_core sim_v2_compile_flags)
add_test(NAME ExperimentFrameworkVerification COMMAND test_experiments)

add_executable(test_collision_acceptance tests/test_collision_acceptance.cpp)
target_link_libraries(test_collision_acceptance PRIVATE sim_core sim_v2_compile_flags)
add_test(NAME CollisionLaboratoryVerification COMMAND test_collision_acceptance)

add_executable(test_omp_correctness tests/test_omp_correctness.cpp)
target_link_libraries(test_omp_correctness PRIVATE sim_omp_v2 sim_core OpenMP::OpenMP_CXX sim_v2_compile_flags)
add_test(NAME OpenMpCorrectnessVerification COMMAND test_omp_correctness)

add_executable(test_spatial_grid tests/test_spatial_grid.cpp)
target_link_libraries(test_spatial_grid PRIVATE sim_omp_v2 sim_core OpenMP::OpenMP_CXX sim_v2_compile_flags)
add_test(NAME SpatialGridVerification COMMAND test_spatial_grid)

if(CUDA_BACKEND_AVAILABLE)
  add_executable(test_cuda_correctness tests/test_cuda_correctness.cpp)
  target_link_libraries(test_cuda_correctness PRIVATE sim_cuda_v2 sim_core sim_v2_compile_flags)
  add_test(NAME CudaCorrectnessVerification COMMAND test_cuda_correctness)

  add_executable(test_cuda_spatial_grid tests/test_cuda_spatial_grid.cpp)
  target_link_libraries(test_cuda_spatial_grid PRIVATE sim_cuda_v2 sim_core sim_v2_compile_flags)
  add_test(NAME CudaSpatialGridVerification COMMAND test_cuda_spatial_grid)

  add_executable(test_cross_backend_verification tests/test_cross_backend_verification.cpp)
  target_link_libraries(test_cross_backend_verification PRIVATE sim_cuda_v2 sim_omp_v2 sim_core OpenMP::OpenMP_CXX sim_v2_compile_flags)
  add_test(NAME CrossBackendVerification COMMAND test_cross_backend_verification)
endif()

if(BUILD_LAB_GUI)
  add_test(NAME LabGuiSmokeTest COMMAND sim_lab --test-headless 60)
endif()

# -----------------------------------------------------------------------------
# Standalone CLI Runners
# -----------------------------------------------------------------------------
add_executable(sim_cpu_ref sim_core/src/main_cpu_ref.cpp)
target_link_libraries(sim_cpu_ref PRIVATE sim_core sim_v2_compile_flags)

add_executable(run_experiments experiments/src/run_experiments.cpp)
target_link_libraries(run_experiments PRIVATE sim_experiments sim_core sim_v2_compile_flags)

add_executable(benchmark_omp sim_omp_v2/src/benchmark_omp.cpp)
target_link_libraries(benchmark_omp PRIVATE sim_omp_v2 sim_core OpenMP::OpenMP_CXX sim_v2_compile_flags)

add_executable(benchmark_broadphase_breakdown sim_omp_v2/src/benchmark_broadphase_breakdown.cpp)
target_link_libraries(benchmark_broadphase_breakdown PRIVATE sim_omp_v2 sim_core OpenMP::OpenMP_CXX sim_v2_compile_flags)

if(CUDA_BACKEND_AVAILABLE)
  add_executable(benchmark_cross_backend sim_cuda_v2/src/benchmark_cross_backend.cpp)
  target_link_libraries(benchmark_cross_backend PRIVATE sim_cuda_v2 sim_omp_v2 sim_core OpenMP::OpenMP_CXX sim_v2_compile_flags)
endif()

add_executable(audit_numerical_solvers experiments/src/audit_numerical_solvers.cpp)
set(AUDIT_LIBS sim_omp_v2 sim_core OpenMP::OpenMP_CXX sim_v2_compile_flags)
if(CUDA_BACKEND_AVAILABLE)
  list(APPEND AUDIT_LIBS sim_cuda_v2)
endif()
target_link_libraries(audit_numerical_solvers PRIVATE ${AUDIT_LIBS})
add_test(NAME NumericalAuditVerification COMMAND audit_numerical_solvers --out ${CMAKE_SOURCE_DIR}/output/audit)

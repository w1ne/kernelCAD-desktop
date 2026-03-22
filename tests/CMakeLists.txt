find_package(Qt6 REQUIRED COMPONENTS Test)

# ── Sketch solver tests ─────────────────────────────────────────────────────
add_executable(test_sketch_solver
  test_SketchSolver.cpp
  ${CMAKE_SOURCE_DIR}/src/sketch/Sketch.cpp
  ${CMAKE_SOURCE_DIR}/src/sketch/SketchSolver.cpp
)
target_include_directories(test_sketch_solver PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sketch_solver PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test)
add_test(NAME SketchSolver COMMAND test_sketch_solver)

# ── Timeline tests ───────────────────────────────────────────────────────────
add_executable(test_timeline
  test_Timeline.cpp
  ${CMAKE_SOURCE_DIR}/src/document/Timeline.cpp
  ${CMAKE_SOURCE_DIR}/src/document/DependencyGraph.cpp
  ${CMAKE_SOURCE_DIR}/src/features/Feature.cpp
)
target_include_directories(test_timeline PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_timeline PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test)
add_test(NAME Timeline COMMAND test_timeline)

# ── DependencyGraph tests ────────────────────────────────────────────────────
add_executable(test_dependency_graph
  test_DependencyGraph.cpp
  ${CMAKE_SOURCE_DIR}/src/document/DependencyGraph.cpp
)
target_include_directories(test_dependency_graph PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_dependency_graph PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test)
add_test(NAME DependencyGraph COMMAND test_dependency_graph)

# ── ParameterStore tests ─────────────────────────────────────────────────────
add_executable(test_paramstore
  test_ParameterStore.cpp
  ${CMAKE_SOURCE_DIR}/src/document/ParameterStore.cpp
)
target_include_directories(test_paramstore PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_paramstore PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test)
add_test(NAME ParameterStore COMMAND test_paramstore)

# ── Sketch button tests ─────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(test_sketch_buttons
  test_SketchButtons.cpp
)
target_include_directories(test_sketch_buttons PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sketch_buttons PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test Qt6::Widgets)
add_test(NAME SketchButtons COMMAND test_sketch_buttons)

# ── OCCTKernel tests ───────────────────────────────────────────────────────
add_executable(test_kernel
  test_OCCTKernel.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/OCCTKernel.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/StableReference.cpp
)
target_include_directories(test_kernel PRIVATE ${CMAKE_SOURCE_DIR}/src ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(test_kernel PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test
  TKernel TKMath TKBRep TKGeomBase TKGeomAlgo TKTopAlgo TKPrim
  TKBool TKFillet TKOffset TKMesh TKSTEP TKIGES TKXSBase TKSTL TKShHealing TKFeat)
add_test(NAME OCCTKernel COMMAND test_kernel)

# ── PluginManager tests ────────────────────────────────────────────────────
add_executable(test_plugin_manager
  test_PluginManager.cpp
  ${CMAKE_SOURCE_DIR}/src/scripting/PluginManager.cpp
)
target_include_directories(test_plugin_manager PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_plugin_manager PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test)
add_test(NAME PluginManager COMMAND test_plugin_manager)

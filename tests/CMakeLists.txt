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


# ── End-to-end workflow tests ────────────────────────────────────────────────
add_executable(test_workflows
  test_Workflows.cpp
  ${CMAKE_SOURCE_DIR}/src/sketch/Sketch.cpp
  ${CMAKE_SOURCE_DIR}/src/sketch/SketchSolver.cpp
  ${CMAKE_SOURCE_DIR}/src/sketch/SketchToShape.cpp
  ${CMAKE_SOURCE_DIR}/src/document/Document.cpp
  ${CMAKE_SOURCE_DIR}/src/document/Timeline.cpp
  ${CMAKE_SOURCE_DIR}/src/document/DependencyGraph.cpp
  ${CMAKE_SOURCE_DIR}/src/document/ParameterStore.cpp
  ${CMAKE_SOURCE_DIR}/src/document/Origin.cpp
  ${CMAKE_SOURCE_DIR}/src/document/CommandHistory.cpp
  ${CMAKE_SOURCE_DIR}/src/document/Commands.cpp
  ${CMAKE_SOURCE_DIR}/src/document/Component.cpp
  ${CMAKE_SOURCE_DIR}/src/document/JointSolver.cpp
  ${CMAKE_SOURCE_DIR}/src/document/JsonReader.cpp
  ${CMAKE_SOURCE_DIR}/src/document/Serializer.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/OCCTKernel.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/BRepModel.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/EntityAttribute.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/BRepQuery.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/StableReference.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/Appearance.cpp
  ${CMAKE_SOURCE_DIR}/src/kernel/MeshBoolean.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ChamferFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/CircularPatternFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/CoilFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/CombineFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ConstructionAxis.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ConstructionPlane.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ConstructionPoint.cpp
  ${CMAKE_SOURCE_DIR}/src/features/DeleteFaceFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/DraftFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ExtrudeFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/Feature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/FilletFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/HoleFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/Joint.cpp
  ${CMAKE_SOURCE_DIR}/src/features/LoftFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/MirrorFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/MoveFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/OffsetFacesFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/PatchFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/PathPatternFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/PressPullFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/RectangularPatternFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ReplaceFaceFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ReverseNormalFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/RevolveFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/RibFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ScaleFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ShellFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/SketchFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/SplitBodyFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/SplitFaceFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/StitchFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/SweepFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ThickenFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/ThreadFeature.cpp
  ${CMAKE_SOURCE_DIR}/src/features/WebFeature.cpp
)
target_include_directories(test_workflows PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${OpenCASCADE_INCLUDE_DIR}
)
target_link_libraries(test_workflows PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test
  TKernel TKMath TKBRep TKGeomBase TKGeomAlgo
  TKTopAlgo TKPrim TKBool TKFillet TKOffset
  TKMesh TKSTEP TKIGES TKXSBase TKSTL TKShHealing TKHLR TKFeat
)
add_test(NAME Workflows COMMAND test_workflows)

# ── DrawingView tests (auto-dimensions, export) ────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS PrintSupport)
add_executable(test_drawing_view
  test_DrawingView.cpp
  ${CMAKE_SOURCE_DIR}/src/ui/DrawingView.cpp
)
target_include_directories(test_drawing_view PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${OpenCASCADE_INCLUDE_DIR}
)
target_link_libraries(test_drawing_view PRIVATE
  kernelcad_compiler_options Qt6::Core Qt6::Test Qt6::Widgets Qt6::Gui Qt6::PrintSupport
  $<$<TARGET_EXISTS:Qt6::Svg>:Qt6::Svg>
  TKernel TKMath TKBRep TKGeomBase TKGeomAlgo TKTopAlgo TKPrim
  TKBool TKFillet TKOffset TKMesh TKSTEP TKIGES TKXSBase TKSTL TKShHealing TKHLR TKFeat
)
add_test(NAME DrawingView COMMAND test_drawing_view)

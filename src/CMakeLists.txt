add_executable(kernelCAD
  main.cpp
  app/Application.cpp
  # kernel
  kernel/OCCTKernel.cpp
  kernel/BRepModel.cpp
  kernel/MeshBoolean.cpp
  kernel/EntityAttribute.cpp
  kernel/BRepQuery.cpp
  kernel/StableReference.cpp
  kernel/Appearance.cpp
  # document
  document/Document.cpp
  document/Timeline.cpp
  document/DependencyGraph.cpp
  document/ParameterStore.cpp
  document/Origin.cpp
  document/CommandHistory.cpp
  document/Commands.cpp
  document/JsonReader.cpp
  document/Serializer.cpp
  document/Component.cpp
  document/JointSolver.cpp
  document/PreviewEngine.cpp
  document/AutoSave.cpp
  document/InteractiveCommands.cpp
  # features
  features/Feature.cpp
  features/ConstructionPlane.cpp
  features/ConstructionAxis.cpp
  features/ConstructionPoint.cpp
  features/ExtrudeFeature.cpp
  features/RevolveFeature.cpp
  features/FilletFeature.cpp
  features/ChamferFeature.cpp
  features/SweepFeature.cpp
  features/LoftFeature.cpp
  features/ShellFeature.cpp
  features/MirrorFeature.cpp
  features/RectangularPatternFeature.cpp
  features/CircularPatternFeature.cpp
  features/HoleFeature.cpp
  features/CombineFeature.cpp
  features/SplitBodyFeature.cpp
  features/OffsetFacesFeature.cpp
  features/MoveFeature.cpp
  features/DraftFeature.cpp
  features/ThickenFeature.cpp
  features/ThreadFeature.cpp
  features/ScaleFeature.cpp
  features/PathPatternFeature.cpp
  features/CoilFeature.cpp
  features/DeleteFaceFeature.cpp
  features/ReplaceFaceFeature.cpp
  features/ReverseNormalFeature.cpp
  features/Joint.cpp
  features/SketchFeature.cpp
  # sketch
  sketch/Sketch.cpp
  sketch/SketchSolver.cpp
  sketch/SketchToShape.cpp
  sketch/DxfImporter.cpp
  sketch/SvgImporter.cpp
  # ui
  ui/MainWindow.cpp
  ui/CommandController.cpp
  ui/CameraController.cpp
  ui/Viewport3D.cpp
  ui/FeatureTree.cpp
  ui/TimelinePanel.cpp
  ui/PropertiesPanel.cpp
  ui/SelectionManager.cpp
  ui/SketchEditor.cpp
  ui/MeasureTool.cpp
  ui/JointCreator.cpp
  ui/CommandDialog.cpp
  ui/FeatureDialog.cpp
  ui/MarkingMenu.cpp
  ui/CommandPalette.cpp
  ui/ViewportManipulator.cpp
  ui/IconFactory.cpp
  ui/IconDrawFunctions.cpp
  ui/ParameterTablePanel.cpp
  ui/PreferencesDialog.cpp
  ui/DrawingView.cpp
  ui/PropertyFormFactory.cpp
)

target_include_directories(kernelCAD PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(kernelCAD PRIVATE
  kernelcad_compiler_options
  Qt6::Core
  Qt6::Widgets
  Qt6::OpenGLWidgets
  Qt6::Gui
  Qt6::PrintSupport
  $<$<TARGET_EXISTS:Qt6::Svg>:Qt6::Svg>
  # OCCT modules
  TKernel TKMath TKBRep TKGeomBase TKGeomAlgo
  TKTopAlgo TKPrim TKBool TKFillet TKOffset
  TKMesh TKSTEP TKIGES TKXSBase TKSTL
  TKShHealing TKHLR
)

# Platform-specific
if(WIN32)
  set_target_properties(kernelCAD PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
  set_target_properties(kernelCAD PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# ── CLI scripting tool (no Qt GUI dependency) ────────────────────────────
add_executable(kernelcad-cli
  scripting/cli_main.cpp
  scripting/ScriptEngine.cpp
  # kernel
  kernel/OCCTKernel.cpp
  kernel/BRepModel.cpp
  kernel/MeshBoolean.cpp
  kernel/EntityAttribute.cpp
  kernel/BRepQuery.cpp
  kernel/StableReference.cpp
  kernel/Appearance.cpp
  # document (no AutoSave — it uses QObject/QTimer)
  document/Document.cpp
  document/Timeline.cpp
  document/DependencyGraph.cpp
  document/ParameterStore.cpp
  document/Origin.cpp
  document/CommandHistory.cpp
  document/Commands.cpp
  document/JsonReader.cpp
  document/Serializer.cpp
  document/Component.cpp
  document/JointSolver.cpp
  document/PreviewEngine.cpp
  document/InteractiveCommands.cpp
  # features
  features/Feature.cpp
  features/ConstructionPlane.cpp
  features/ConstructionAxis.cpp
  features/ConstructionPoint.cpp
  features/ExtrudeFeature.cpp
  features/RevolveFeature.cpp
  features/FilletFeature.cpp
  features/ChamferFeature.cpp
  features/SweepFeature.cpp
  features/LoftFeature.cpp
  features/ShellFeature.cpp
  features/MirrorFeature.cpp
  features/RectangularPatternFeature.cpp
  features/CircularPatternFeature.cpp
  features/HoleFeature.cpp
  features/CombineFeature.cpp
  features/SplitBodyFeature.cpp
  features/OffsetFacesFeature.cpp
  features/MoveFeature.cpp
  features/DraftFeature.cpp
  features/ThickenFeature.cpp
  features/ThreadFeature.cpp
  features/ScaleFeature.cpp
  features/PathPatternFeature.cpp
  features/CoilFeature.cpp
  features/DeleteFaceFeature.cpp
  features/ReplaceFaceFeature.cpp
  features/ReverseNormalFeature.cpp
  features/Joint.cpp
  features/SketchFeature.cpp
  # sketch
  sketch/Sketch.cpp
  sketch/SketchSolver.cpp
  sketch/SketchToShape.cpp
)

target_include_directories(kernelcad-cli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(kernelcad-cli PRIVATE
  kernelcad_compiler_options
  # OCCT modules
  TKernel TKMath TKBRep TKGeomBase TKGeomAlgo
  TKTopAlgo TKPrim TKBool TKFillet TKOffset
  TKMesh TKSTEP TKIGES TKXSBase TKSTL
  TKShHealing
)

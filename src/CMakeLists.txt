add_executable(kernelCAD
  main.cpp
  app/Application.cpp
  # kernel
  kernel/OCCTKernel.cpp
  kernel/BRepModel.cpp
  # document
  document/Document.cpp
  document/Timeline.cpp
  document/ParameterStore.cpp
  # features
  features/Feature.cpp
  features/ExtrudeFeature.cpp
  # ui
  ui/MainWindow.cpp
  ui/Viewport3D.cpp
  ui/FeatureTree.cpp
  ui/TimelinePanel.cpp
  ui/PropertiesPanel.cpp
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
  # OCCT modules
  TKernel TKMath TKBRep TKGeomBase TKGeomAlgo
  TKTopAlgo TKPrim TKBool TKFillet TKOffset
  TKMesh TKSTEP TKXSBase TKSTL
)

# Platform-specific
if(WIN32)
  set_target_properties(kernelCAD PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
  set_target_properties(kernelCAD PROPERTIES MACOSX_BUNDLE TRUE)
endif()

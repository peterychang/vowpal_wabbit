if(NOT DEFINED PY_VERSION)
  set(PY_VERSION 2.7)
  message(WARNING "No PY_VERSION specified, Python ${PY_VERSION} will be used for VowpalWabbit Python bindings")
else()
  message(STATUS "Python ${PY_VERSION} will be used for VowpalWabbit Python bindings")
endif()
string(REPLACE . "" PY_VERSION_STRIPPED ${PY_VERSION})

find_package(PythonInterp ${PY_VERSION} REQUIRED)
find_package(PythonLibs ${PY_VERSION} REQUIRED)

if (NOT WIN32)
  # Use position independent code for all targets in this directory
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT WIN32)
  set(EXPORT_DYNAMIC -Wl,--export-dynamic)
endif()

# Note: Python bindings previously built under c++0x

# create the lib
add_library(pylibvw SHARED pylibvw.cc)

set_target_properties(pylibvw PROPERTIES PREFIX "")
if (NOT WIN32)
  # OSX builds a .dylib and then attempts to find a .so. Just build a .so file and things work
  set_target_properties(pylibvw PROPERTIES SUFFIX ".so")
else()
  # Force Windows to build with a .pyd extension
  set_target_properties(pylibvw PROPERTIES SUFFIX ".pyd")
endif()
target_link_libraries(pylibvw PUBLIC ${PYTHON_LIBRARIES} vw allreduce)
target_include_directories(pylibvw PRIVATE ${PYTHON_INCLUDE_PATH} ${CMAKE_SOURCE_DIR}/external/pybind11/include)
target_compile_options(pylibvw PRIVATE ${EXPORT_DYNAMIC})

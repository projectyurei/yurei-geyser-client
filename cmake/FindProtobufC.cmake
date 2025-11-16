find_package(PkgConfig REQUIRED)
pkg_check_modules(ProtobufC QUIET libprotobuf-c)
if(NOT ProtobufC_FOUND)
  pkg_check_modules(ProtobufC REQUIRED protobuf-c)
endif()

find_path(ProtobufC_INCLUDE_DIR
  NAMES protobuf-c/protobuf-c.h
  HINTS ${ProtobufC_INCLUDE_DIRS})

find_library(ProtobufC_LIBRARY
  NAMES protobuf-c
  HINTS ${ProtobufC_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ProtobufC DEFAULT_MSG ProtobufC_LIBRARY ProtobufC_INCLUDE_DIR)

if(NOT TARGET ProtobufC::protobuf-c)
  add_library(ProtobufC::protobuf-c UNKNOWN IMPORTED)
  set_target_properties(ProtobufC::protobuf-c PROPERTIES
    IMPORTED_LOCATION "${ProtobufC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ProtobufC_INCLUDE_DIR}"
  )
endif()

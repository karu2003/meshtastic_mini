# Include nanopb runtime from submodule third_party/nanopb (no generator).
# Call from project root: include(cmake/NanopbRuntime.cmake)
# Requires: git submodule update --init --recursive

set(NANOPB_ROOT ${CMAKE_SOURCE_DIR}/third_party/nanopb)
if(NOT EXISTS ${NANOPB_ROOT}/pb.h)
  message(FATAL_ERROR "nanopb submodule not found. Run: git submodule update --init --recursive")
endif()

set(NANOPB_RUNTIME_SRCS
  ${NANOPB_ROOT}/pb_common.c
  ${NANOPB_ROOT}/pb_encode.c
  ${NANOPB_ROOT}/pb_decode.c
)
set(NANOPB_INCLUDE_DIR ${NANOPB_ROOT})

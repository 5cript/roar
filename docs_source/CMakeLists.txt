find_package(Doxygen REQUIRED)

get_target_property(PUBLIC_HEADER_DIR roar INTERFACE_INCLUDE_DIRECTORIES)
file(GLOB_RECURSE PUBLIC_HEADERS ${PUBLIC_HEADER_DIR}/*.hpp)

set(DOCS_DIR ${CMAKE_BINARY_DIR}/docs)
set(DOXYGEN_OUTPUT_DIR ${DOCS_DIR}/doxygen)
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/html/index.html)
set(SPHINX_DIR ${DOCS_DIR})
set(SPHINX_INDEX_FILE ${DOCS_DIR}/index.html)

# Directory Structure
add_custom_command(
    OUTPUT ${SPHINX_DIR}
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DOXYGEN_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DOXYGEN_OUTPUT_DIR}")

# Doxygen Command
add_custom_command(
    OUTPUT ${DOXYGEN_INDEX_FILE}
    DEPENDS ${PUBLIC_HEADERS}
    DEPENDS ${SPHINX_DIR}
    COMMAND BUILD_DIR=${DOXYGEN_OUTPUT_DIR} ${DOXYGEN_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/doxyfile
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${DOXYGEN_OUTPUT_DIR}/html ${SPHINX_DIR}/doxygen
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${DOXYGEN_OUTPUT_DIR}/html
    COMMAND /usr/bin/env python3 ${CMAKE_SOURCE_DIR}/docs_source/scripts/postprocess.py --input
            ${SPHINX_DIR}/doxygen
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    MAIN_DEPENDENCY doxyfile
    COMMENT "Generating doxygen docs and process them for sphinx")

# Sphinx Command
add_custom_command(
    OUTPUT ${SPHINX_INDEX_FILE}
    DEPENDS ${SPHINX_DIR}
    # DEPENDS ${DOXYGEN_INDEX_FILE}
    COMMAND sphinx-build -M "html" "${CMAKE_CURRENT_SOURCE_DIR}/sphinx"
            "${DOCS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SPHINX_DIR}/html ${DOCS_DIR}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${SPHINX_DIR}/html
    WORKING_DIRECTORY ${SPHINX_DIR})

add_custom_target(
    documentation
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/scripts/servethis.py"
            "${DOCS_DIR}/servethis.py"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/auxiliary/.nojekyll"
            "${DOCS_DIR}/.nojekyll"
  DEPENDS ${SPHINX_INDEX_FILE})

add_custom_target(
    doxygen
    DEPENDS ${DOXYGEN_INDEX_FILE})

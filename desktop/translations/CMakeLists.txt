include(CreateTranslations)
include(ImportQtTranslations)

file(GLOB ikeback_translations ${CMAKE_CURRENT_SOURCE_DIR}/*.ts)
file(GLOB_RECURSE ikeback_sources ${CMAKE_SOURCE_DIR}/*.cpp ${CMAKE_SOURCE_DIR}/*.h ${CMAKE_SOURCE_DIR}/*.ui ${CMAKE_SOURCE_DIR}/*.qml)
string(REGEX REPLACE "${CMAKE_SOURCE_DIR}/3rdparty[^;]+;?" "" ikeback_sources "${ikeback_sources}")
string(REGEX REPLACE "${CMAKE_SOURCE_DIR}/addons[^;]+;?" "" ikeback_sources "${ikeback_sources}")

create_translations(ikeback "${ikeback_translations}" "${ikeback_sources}")
import_qt_translations()

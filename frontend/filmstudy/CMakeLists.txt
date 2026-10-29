add_library(filmstudy-core STATIC Catalog.cpp Catalog.hpp MediaImporter.cpp MediaImporter.hpp SchemaV2.generated.hpp)
add_library(filmstudy-sqlite STATIC third_party/sqlite/sqlite3.c third_party/sqlite/sqlite3.h)
target_compile_definitions(filmstudy-sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION=1 SQLITE_DQS=0)
set_target_properties(
  filmstudy-sqlite PROPERTIES COMPILE_WARNING_AS_ERROR OFF XCODE_ATTRIBUTE_GCC_TREAT_WARNINGS_AS_ERRORS NO
                              POSITION_INDEPENDENT_CODE ON
)
target_link_libraries(filmstudy-core PUBLIC Qt::Core PRIVATE filmstudy-sqlite FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil)
target_include_directories(filmstudy-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(filmstudy-core PROPERTIES FOLDER frontend POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_library(filmstudy-ui MODULE FilmStudyModule.cpp)
target_link_libraries(filmstudy-ui PRIVATE filmstudy-core OBS::libobs OBS::frontend-api Qt::Widgets)
set_target_properties_obs(filmstudy-ui PROPERTIES FOLDER plugins/filmstudy PREFIX "" AUTOMOC ON)

add_executable(filmstudy-catalog-smoke ../../tests/filmstudy/catalog_smoke.cpp)
target_link_libraries(filmstudy-catalog-smoke PRIVATE filmstudy-core)
set_target_properties(filmstudy-catalog-smoke PROPERTIES FOLDER frontend)
if(OS_WINDOWS)
  add_custom_command(
    TARGET filmstudy-catalog-smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt::Core> $<TARGET_FILE_DIR:filmstudy-catalog-smoke>
  )
endif()

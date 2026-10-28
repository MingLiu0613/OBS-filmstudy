add_library(filmstudy-core STATIC Catalog.cpp Catalog.hpp SchemaV2.generated.hpp)
add_library(filmstudy-sqlite STATIC third_party/sqlite/sqlite3.c third_party/sqlite/sqlite3.h)
target_compile_definitions(filmstudy-sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION=1 SQLITE_DQS=0)
target_link_libraries(filmstudy-core PUBLIC Qt::Core PRIVATE filmstudy-sqlite)
target_include_directories(filmstudy-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(filmstudy-core PROPERTIES FOLDER frontend)

add_executable(filmstudy-catalog-smoke ../../tests/filmstudy/catalog_smoke.cpp)
target_link_libraries(filmstudy-catalog-smoke PRIVATE filmstudy-core)
set_target_properties(filmstudy-catalog-smoke PROPERTIES FOLDER frontend)
if(OS_WINDOWS)
  add_custom_command(
    TARGET filmstudy-catalog-smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt::Core> $<TARGET_FILE_DIR:filmstudy-catalog-smoke>
  )
endif()

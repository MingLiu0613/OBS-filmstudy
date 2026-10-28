find_package(Qt6 REQUIRED COMPONENTS Sql)

add_library(filmstudy-core STATIC Catalog.cpp Catalog.hpp SchemaV2.generated.hpp)
target_link_libraries(filmstudy-core PUBLIC Qt::Core Qt::Sql)
target_include_directories(filmstudy-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(filmstudy-core PROPERTIES FOLDER frontend)

add_library(dlsim_core STATIC
    Model.h Model.cpp
    TcCommandBuilder.h TcCommandBuilder.cpp
    Parsers.h Parsers.cpp
)
target_include_directories(dlsim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dlsim_core PUBLIC Qt6::Core)
set_target_properties(dlsim_core PROPERTIES AUTOMOC OFF)

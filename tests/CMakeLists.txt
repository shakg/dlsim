foreach(name tst_tccommandbuilder tst_parsers tst_model tst_errors tst_simple tst_dropouts tst_curve tst_sockethealth)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE dlsim_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

add_executable(tst_presetstore tst_presetstore.cpp ../src/PresetStore.h ../src/PresetStore.cpp)
target_include_directories(tst_presetstore PRIVATE ../src)
target_link_libraries(tst_presetstore PRIVATE dlsim_core Qt6::Test)
qt_add_resources(tst_presetstore "presets"
    PREFIX "/presets"
    BASE ../presets
    FILES ../presets/presets.json ../presets/scenarios.json ../presets/templates.json
)
add_test(NAME tst_presetstore COMMAND tst_presetstore)

add_executable(tst_templates tst_templates.cpp)
target_link_libraries(tst_templates PRIVATE dlsim_core Qt6::Test)
target_compile_definitions(tst_templates PRIVATE DLSIM_TEMPLATES_JSON="${CMAKE_SOURCE_DIR}/presets/templates.json")
add_test(NAME tst_templates COMMAND tst_templates)

add_subdirectory(core)

qt_add_executable(dlsim
    main.cpp
    EventLog.h EventLog.cpp
    HelperClient.h HelperClient.cpp
    LinkController.h LinkController.cpp
    PresetStore.h PresetStore.cpp
    ScenarioEngine.h ScenarioEngine.cpp
    SocketMonitor.h SocketMonitor.cpp
    StatsPoller.h StatsPoller.cpp
    SystemInfo.h SystemInfo.cpp
)

target_compile_definitions(dlsim PRIVATE DLSIM_HELPER_INSTALL_PATH="${DLSIM_HELPER_INSTALL_PATH}")
target_link_libraries(dlsim PRIVATE
    dlsim_core
    Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Charts
)

set(DLSIM_QML_FILES
    ../qml/Main.qml
    ../qml/Theme.qml
    ../qml/InterfacesPage.qml
    ../qml/LinkPage.qml
    ../qml/ProfileEditor.qml
    ../qml/ParamRow.qml
    ../qml/ClassesPanel.qml
    ../qml/ScenarioPage.qml
    ../qml/MonitorPage.qml
    ../qml/LogPanel.qml
    ../qml/CommandsPanel.qml
    ../qml/Section.qml
    ../qml/BoundCombo.qml
    ../qml/StatChart.qml
    ../qml/qmldir
)
qt_add_resources(dlsim "qml"
    PREFIX "/qml"
    BASE ../qml
    FILES ${DLSIM_QML_FILES}
)
qt_add_resources(dlsim "presets"
    PREFIX "/presets"
    BASE ../presets
    FILES ../presets/presets.json ../presets/scenarios.json
)

install(TARGETS dlsim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ../dist/dlsim.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ../dist/dlsim.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

cmake_minimum_required(VERSION 3.16)

project(ApplicationGeometryRecovery VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets Network LinguistTools)

# GitHub repository with the releases for the update check, as owner/name
set(AGR_UPDATE_REPOSITORY "Halikanese/application-geomertry-recovery" CACHE STRING
    "GitHub repository (owner/name) with the releases")

qt_standard_project_setup(REQUIRES 6.12
    I18N_SOURCE_LANGUAGE en
    I18N_TRANSLATED_LANGUAGES de
)

qt_add_executable(ApplicationGeometryRecovery
    main.cpp
    XmlSettingsBackend.h
    XmlSettingsBackend.cpp
    WindowInfo.h
    WindowGeometry.h
    WindowGeometry.cpp
    ProfileStore.h
    ProfileStore.cpp
    ProfileModel.h
    ProfileModel.cpp
    TrayController.h
    TrayController.cpp
    MainWindow.h
    MainWindow.cpp
    MainWindow.ui
    AppIcon.h
    AppIcon.cpp
    ApplicationStarter.h
    ApplicationStarter.cpp
    WindowWatcher.h
    WindowWatcher.cpp
    UpdateChecker.h
    UpdateChecker.cpp
)

set_target_properties(ApplicationGeometryRecovery PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(ApplicationGeometryRecovery
    PRIVATE Qt6::Widgets Qt6::Network
)

# Version of the application (also of the installer) and update source
target_compile_definitions(ApplicationGeometryRecovery PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    AGR_UPDATE_REPOSITORY="${AGR_UPDATE_REPOSITORY}"
)

# Not compiled, listed so that lupdate also extracts the installer texts
target_sources(ApplicationGeometryRecovery PRIVATE
    installer/installscript.qs
    installer/controlscript.qs
    installer/ShortcutsPage.ui
)
# The installer loads the form at runtime, no ui_*.h needed
set_source_files_properties(installer/ShortcutsPage.ui PROPERTIES SKIP_AUTOUIC ON)

# Translations in i18n/*.ts, refresh them with the update_translations target.
# With QM_FILES_OUTPUT_VARIABLE the .qm files are not embedded automatically
# (they are also needed for the installer), so they are added under :/i18n here.
qt_add_translations(ApplicationGeometryRecovery
    TS_FILE_DIR i18n
    QM_FILES_OUTPUT_VARIABLE qmFiles
)
qt_add_resources(ApplicationGeometryRecovery "translations"
    PREFIX "/i18n"
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${qmFiles}
)

if (WIN32)
    # Window enumeration (user32), DWM cloaking (dwmapi), monitor DPI (shcore)
    target_link_libraries(ApplicationGeometryRecovery PRIVATE user32 dwmapi shcore)
    target_compile_definitions(ApplicationGeometryRecovery PRIVATE UNICODE _UNICODE NOMINMAX)
    # Icon of the EXE (Explorer, Start menu and autostart shortcuts), generated
    # from appIcon()
    target_sources(ApplicationGeometryRecovery PRIVATE ApplicationGeometryRecovery.rc)
endif()

install(TARGETS ApplicationGeometryRecovery
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Application
)

# On install, copies the required Qt DLLs, plugins and
# MinGW runtime libraries next to the EXE (windeployqt)
qt_generate_deploy_app_script(
    TARGET ApplicationGeometryRecovery
    OUTPUT_SCRIPT deployScript
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deployScript} COMPONENT Application)
install(FILES LICENSE DESTINATION . COMPONENT Application)

# Installer via the "package" target. With the Qt Installer Framework installed
# (Maintenance Tool: Developer and Designer Tools) this creates a setup program,
# otherwise a ZIP archive.
set(CPACK_PACKAGE_NAME "ApplicationGeometryRecovery")
set(CPACK_PACKAGE_VENDOR "LightningSoftware")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Saves and restores window geometries in profiles")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ApplicationGeometryRecovery")
set(CPACK_PACKAGE_EXECUTABLES "ApplicationGeometryRecovery" "Application Geometry Recovery")

file(GLOB IFW_CANDIDATES "${QT6_INSTALL_PREFIX}/../../Tools/QtInstallerFramework/*/bin")
find_program(IFW_BINARYCREATOR binarycreator HINTS ${IFW_CANDIDATES})
if (IFW_BINARYCREATOR)
    get_filename_component(CPACK_IFW_ROOT "${IFW_BINARYCREATOR}/../.." ABSOLUTE)
    set(CPACK_GENERATOR IFW)
    set(CPACK_IFW_PACKAGE_TITLE "Application Geometry Recovery")
    # Adds the program description to the welcome page. The German texts
    # from i18n/*.ts are embedded, because the welcome page is shown before
    # the component's .qm files are loaded.
    include(installer/EmbedTranslations.cmake)
    file(GLOB tsFiles ${CMAKE_CURRENT_SOURCE_DIR}/i18n/*.ts)
    ifw_embed_translations(IFW_TRANSLATIONS installscript ${tsFiles})
    configure_file(installer/controlscript.qs installer/controlscript.qs @ONLY)
    set(CPACK_IFW_PACKAGE_CONTROL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/installer/controlscript.qs)
    # Readable texts in light and dark mode: a light style sheet for all
    # widgets and the Classic style, whose page titles are part of the page.
    # The Aero and Modern headers follow the system colors and cannot be
    # styled, so their title is unreadable in one of the two modes.
    set(CPACK_IFW_PACKAGE_STYLE_SHEET ${CMAKE_CURRENT_SOURCE_DIR}/installer/style.qss)
    set(CPACK_IFW_PACKAGE_WIZARD_STYLE Classic)
    set(CPACK_IFW_PACKAGE_WIZARD_DEFAULT_HEIGHT 520)
    set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "Application Geometry Recovery")
    set(CPACK_IFW_TARGET_DIRECTORY "@ApplicationsDirX64@/ApplicationGeometryRecovery")
    # Check box "Run ... now" on the last page, text and default in controlscript.qs
    set(CPACK_IFW_PACKAGE_RUN_PROGRAM "@TargetDir@/bin/ApplicationGeometryRecovery.exe")
    message(STATUS "Installer: Qt Installer Framework (${CPACK_IFW_ROOT})")
else()
    set(CPACK_GENERATOR ZIP)
    message(STATUS "Installer: Qt Installer Framework not found, creating ZIP")
endif()

include(CPack)

if (IFW_BINARYCREATOR)
    include(CPackIFW)
    # The installer expects translations named after the language (de.qm).
    # The application's .qm also contains the texts of installscript.qs.
    set(ifwQmFiles)
    set(copyCommands)
    foreach(qmFile IN LISTS qmFiles)
        string(REGEX MATCH "_([a-zA-Z_]+)\\.qm$" _ "${qmFile}")
        if (CMAKE_MATCH_1 AND NOT CMAKE_MATCH_1 STREQUAL "en")
            set(ifwQm ${CMAKE_CURRENT_BINARY_DIR}/installer/${CMAKE_MATCH_1}.qm)
            # CPackIFW checks at configure time that the file exists, the
            # content is copied on every build
            if (NOT EXISTS ${ifwQm})
                file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/installer)
                file(TOUCH ${ifwQm})
            endif()
            list(APPEND copyCommands COMMAND ${CMAKE_COMMAND} -E copy_if_different ${qmFile} ${ifwQm})
            list(APPEND ifwQmFiles ${ifwQm})
        endif()
    endforeach()
    add_custom_target(ifw_translations ALL ${copyCommands} DEPENDS ${qmFiles})

    # Description, license, shortcut options and shortcuts via a component install script
    cpack_add_component(Application
        DISPLAY_NAME "Application Geometry Recovery"
        DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}"
        REQUIRED
    )
    cpack_ifw_configure_component(Application
        SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/installer/installscript.qs
        TRANSLATIONS ${ifwQmFiles}
        USER_INTERFACES ${CMAKE_CURRENT_SOURCE_DIR}/installer/ShortcutsPage.ui
        LICENSES "MIT License" ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    )
endif()

cmake_minimum_required(VERSION 3.16)
project(Survive LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- Sources ----------
set(SOURCES
    src/main.cpp
    src/Game.cpp
    src/GameObject.cpp
    src/Player.cpp
    src/Enemy.cpp
    src/FogEnemy.cpp
    src/Physics.cpp
    src/Pickup.cpp
    src/Powerup.cpp
    src/DebugDrawer.cpp
)

set(HEADERS
    src/Game.h
    src/GameObject.h
    src/Player.h
    src/Enemy.h
    src/FogEnemy.h
    src/Physics.h
    src/Pickup.h
    src/Powerup.h
    src/InputHandler.h
    src/DebugDrawer.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# ---------- Include directories ----------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/irrlicht-1.8.5/include
    ${CMAKE_SOURCE_DIR}/libs/irrKlang/include
    ${CMAKE_SOURCE_DIR}/libs/bullet3/include
)

# ---------- Library paths ----------
set(IRRLICHT_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/irrlicht-1.8.5/lib/Win64-visualStudio)
set(IRRKLANG_LIB_DIR ${CMAKE_SOURCE_DIR}/libs/irrKlang/lib/x64)
set(BULLET_LIB_DIR   ${CMAKE_SOURCE_DIR}/libs/bullet3/lib)

# ---------- Link libraries ----------
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${IRRLICHT_LIB_DIR}/Irrlicht.lib
    ${IRRKLANG_LIB_DIR}/irrKlang.lib
    ${BULLET_LIB_DIR}/$<CONFIG>/BulletDynamics.lib
    ${BULLET_LIB_DIR}/$<CONFIG>/BulletCollision.lib
    ${BULLET_LIB_DIR}/$<CONFIG>/LinearMath.lib
)

# ---------- Preprocessor definitions ----------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32
    _CONSOLE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# ---------- MSVC compiler flags ----------
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3 /sdl /permissive-
        $<$<CONFIG:Release>:/Gy /Oi>
    )
    # Whole program optimization for Release
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
endif()

# ---------- Output directory ----------
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR}/bin/x64/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin/x64/Release
)

# ---------- Visual Studio debugger working directory ----------
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ---------- Copy DLLs to output directory ----------
set(RUNTIME_DLLS
    ${CMAKE_SOURCE_DIR}/libs/irrlicht-1.8.5/bin/Irrlicht.dll
    ${CMAKE_SOURCE_DIR}/libs/irrKlang/bin/irrKlang.dll
    ${CMAKE_SOURCE_DIR}/libs/irrKlang/bin/ikpFlac.dll
    ${CMAKE_SOURCE_DIR}/libs/irrKlang/bin/ikpMP3.dll
)

foreach(DLL_FILE ${RUNTIME_DLLS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL_FILE}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying ${DLL_FILE}"
    )
endforeach()

# ---------- Copy assets to output directory ----------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying assets directory"
)

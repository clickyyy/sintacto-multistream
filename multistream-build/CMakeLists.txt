cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

# OBS MultiStream Hub - Creado por Sintacto (@sintacto12)
# Este plugin necesita el frontend de OBS (dock) y Qt (interfaz).
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/destination.cpp
    src/destination.hpp
    src/secure_store.cpp
    src/secure_store.hpp
    src/encoder_pool.cpp
    src/encoder_pool.hpp
    src/output_manager.cpp
    src/output_manager.hpp
    src/obs_raii.hpp
    src/dock.cpp
    src/dock.hpp
    src/donations.hpp
    src/donations.cpp
    src/vertical_panel.hpp
    src/vertical_panel.cpp
    src/vertical_canvas.hpp
    src/vertical_canvas.cpp
)

if(WIN32)
  # Cifrado de claves con Windows DPAPI
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE crypt32)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
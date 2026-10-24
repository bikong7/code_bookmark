cmake_minimum_required(VERSION 3.16)

project(CodeBookmark
  VERSION 1.0.0
  DESCRIPTION "Save selected code snippets with notes into Markdown"
  HOMEPAGE_URL "https://example.invalid/codebookmark"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)

# Qt Creator 20.0.2 cmake package expects Qt6::TaskTree (Qt 6.11+).
# Provide a stub INTERFACE target when building against older Qt.
if(NOT TARGET Qt6::TaskTree)
  add_library(Qt6::TaskTree INTERFACE IMPORTED)
endif()

find_package(QtCreator REQUIRED)

add_qtc_plugin(CodeBookmark
  VERSION 19.0.2
  COMPAT_VERSION 19.0.0
  PLUGIN_CLASS CodeBookmarkPlugin
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::TextEditor
  SOURCES
    codebookmarkplugin.cpp
    codebookmarkplugin.h
    bookmarkmanager.cpp
    bookmarkmanager.h
    bookmarkpane.cpp
    bookmarkpane.h
    bookmarkoptionspage.cpp
    bookmarkoptionspage.h
)

target_link_libraries(CodeBookmark
  PRIVATE
    Qt6::Widgets
    QtCreator::Core
    QtCreator::TextEditor
)

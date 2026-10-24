// Copyright (C) 2026 CodeBookmark contributors.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <extensionsystem/iplugin.h>

class QAction;

namespace Core {
class IOutputPane;
}

namespace CodeBookmark {

class BookmarkManager;

namespace Internal {

class BookmarkPane;
class BookmarkOptionsPage;

class CodeBookmarkPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "CodeBookmark.json")

public:
    CodeBookmarkPlugin();
    ~CodeBookmarkPlugin() override;

    Utils::Result<> initialize(const QStringList &arguments) override;
    void extensionsInitialized() override;
    ShutdownFlag aboutToShutdown() override;

private:
    void addBookmark();
    void openBookmarkFile();
    void showBookmarksPane();

    BookmarkManager *m_manager = nullptr;
    BookmarkPane *m_pane = nullptr;
    BookmarkOptionsPage *m_optionsPage = nullptr;
    QAction *m_addAction = nullptr;
    QAction *m_openFileAction = nullptr;
    QAction *m_showPaneAction = nullptr;
};

} // namespace Internal
} // namespace CodeBookmark

// Copyright (C) 2026 CodeBookmark contributors.
// SPDX-License-Identifier: GPL-3.0-only

#include "codebookmarkplugin.h"

#include "bookmarkmanager.h"
#include "bookmarkoptionspage.h"
#include "bookmarkpane.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>
#include <texteditor/texteditorconstants.h>

#include <utils/qtcsettings.h>

#include <QAction>
#include <QDir>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QTextBlock>
#include <QTextDocument>

using namespace Core;
using namespace Utils;

namespace CodeBookmark {
namespace Internal {

static const char kAddActionId[] = "CodeBookmark.AddBookmark";
static const char kOpenFileActionId[] = "CodeBookmark.OpenBookmarkFile";
static const char kShowPaneActionId[] = "CodeBookmark.ShowBookmarksPane";

CodeBookmarkPlugin::CodeBookmarkPlugin()
{
    m_manager = new BookmarkManager(this);
}

CodeBookmarkPlugin::~CodeBookmarkPlugin() = default;

Utils::Result<> CodeBookmarkPlugin::initialize(const QStringList &arguments)
{
    Q_UNUSED(arguments)

    // Register actions.
    m_addAction = new QAction(tr("Add to Code Bookmarks"), this);
    Command *addCmd = ActionManager::registerAction(
        m_addAction, kAddActionId, Context(Constants::C_GLOBAL));
    addCmd->setDefaultKeySequence(QKeySequence("Ctrl+Alt+B"));
    connect(m_addAction, &QAction::triggered, this, &CodeBookmarkPlugin::addBookmark);

    m_openFileAction = new QAction(tr("Open Code Bookmarks File"), this);
    Command *openFileCmd = ActionManager::registerAction(
        m_openFileAction, kOpenFileActionId, Context(Constants::C_GLOBAL));
    connect(m_openFileAction, &QAction::triggered, this, &CodeBookmarkPlugin::openBookmarkFile);

    m_showPaneAction = new QAction(tr("Code Bookmarks"), this);
    Command *showPaneCmd = ActionManager::registerAction(
        m_showPaneAction, kShowPaneActionId, Context(Constants::C_GLOBAL));
    connect(m_showPaneAction, &QAction::triggered, this, &CodeBookmarkPlugin::showBookmarksPane);

    // Tools menu.
    if (ActionContainer *toolsMenu = ActionManager::actionContainer(Constants::M_TOOLS)) {
        toolsMenu->addAction(addCmd, Constants::G_TOOLS_DEBUG);
        toolsMenu->addAction(openFileCmd, Constants::G_TOOLS_DEBUG);
    }

    // Editor context menu.
    if (ActionContainer *editorContextMenu =
            ActionManager::actionContainer(TextEditor::Constants::M_STANDARDCONTEXTMENU)) {
        editorContextMenu->addAction(addCmd, TextEditor::Constants::G_COPYPASTE);
    }

    // Options page.
    m_optionsPage = new BookmarkOptionsPage(m_manager);

    return {};
}

void CodeBookmarkPlugin::extensionsInitialized()
{
    m_pane = new BookmarkPane(m_manager, this);
}

ExtensionSystem::IPlugin::ShutdownFlag CodeBookmarkPlugin::aboutToShutdown()
{
    ActionManager::unregisterAction(m_addAction, kAddActionId);
    ActionManager::unregisterAction(m_openFileAction, kOpenFileActionId);
    ActionManager::unregisterAction(m_showPaneAction, kShowPaneActionId);

    delete m_optionsPage;
    m_optionsPage = nullptr;

    delete m_pane;
    m_pane = nullptr;

    return SynchronousShutdown;
}

void CodeBookmarkPlugin::addBookmark()
{
    TextEditor::BaseTextEditor *editor = TextEditor::BaseTextEditor::currentTextEditor();
    if (!editor) {
        QMessageBox::information(ICore::dialogParent(),
                                 tr("Code Bookmarks"),
                                 tr("No text editor is open."));
        return;
    }

    const QString selectedText = editor->selectedText();
    const FilePath filePath = editor->textDocument()->filePath();
    if (filePath.isEmpty()) {
        QMessageBox::information(ICore::dialogParent(),
                                 tr("Code Bookmarks"),
                                 tr("The current file has not been saved yet."));
        return;
    }

    // Line range of the selection (1-based).
    const QTextCursor cursor = editor->textCursor();
    QTextCursor startCursor = cursor;
    startCursor.setPosition(cursor.selectionStart());
    QTextCursor endCursor = cursor;
    endCursor.setPosition(cursor.selectionEnd());
    const int startLine = startCursor.blockNumber() + 1;
    const int endLine = endCursor.blockNumber() + 1;

    // Content: use selected text (truncated), fallback to filename
    QString title = selectedText.trimmed();
    if (title.isEmpty()) {
        title = filePath.fileName();
    } else if (title.length() > 80) {
        title = title.left(80) + "...";
    }

    // Skip if already bookmarked (same file + same start line + same trimmed code).
    const QString trimmedCode = selectedText.trimmed();
    if (m_manager->findDuplicate(filePath, startLine, trimmedCode))
        return;

    BookmarkEntry entry;
    entry.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    entry.title = title;
    entry.note = QString();
    entry.filePath = filePath;
    entry.startLine = startLine;
    entry.endLine = endLine;
    entry.language = BookmarkManager::languageForFile(filePath);
    entry.code = selectedText;

    if (m_manager->addBookmark(entry)) {
        if (m_pane)
            m_pane->popup(IOutputPane::WithFocus | IOutputPane::ModeSwitch);
    } else {
        QMessageBox::warning(ICore::dialogParent(),
                             tr("Code Bookmarks"),
                             tr("Failed to write bookmark to:\n%1")
                                 .arg(QDir::toNativeSeparators(
                                     m_manager->filePath().toUserOutput())));
    }
}

void CodeBookmarkPlugin::openBookmarkFile()
{
    const FilePath path = m_manager->filePath();
    EditorManager::openEditor(path);
}

void CodeBookmarkPlugin::showBookmarksPane()
{
    if (m_pane)
        m_pane->popup(IOutputPane::WithFocus | IOutputPane::ModeSwitch);
}

} // namespace Internal
} // namespace CodeBookmark

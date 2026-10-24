// Copyright (C) 2026 CodeBookmark contributors.
// SPDX-License-Identifier: GPL-3.0-only

#include "bookmarkoptionspage.h"

#include "bookmarkmanager.h"

#include <utils/pathchooser.h>

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

using namespace Core;
using namespace Utils;

namespace CodeBookmark {
namespace Internal {

class BookmarkOptionsPageWidget : public IOptionsPageWidget
{
public:
    explicit BookmarkOptionsPageWidget(BookmarkManager *manager)
        : m_manager(manager)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        m_pathChooser = new PathChooser(this);
        m_pathChooser->setExpectedKind(PathChooser::SaveFile);
        m_pathChooser->setPromptDialogTitle(
            QCoreApplication::translate("CodeBookmark", "Code Bookmarks File"));
        m_pathChooser->setPromptDialogFilter(
            QCoreApplication::translate("CodeBookmark", "Markdown files (*.md);;All files (*)"));
        m_pathChooser->setFilePath(m_manager->filePath());

        auto *row = new QHBoxLayout;
        auto *label = new QLabel(
            QCoreApplication::translate("CodeBookmark", "Markdown file:"), this);
        row->addWidget(label);
        row->addWidget(m_pathChooser, 1);
        layout->addLayout(row);
        layout->addStretch();
    }

    void apply() override
    {
        m_manager->setFilePath(m_pathChooser->filePath());
    }

    bool isDirty() const override
    {
        return m_pathChooser->filePath() != m_manager->filePath();
    }

private:
    BookmarkManager *m_manager;
    PathChooser *m_pathChooser = nullptr;
};

BookmarkOptionsPage::BookmarkOptionsPage(BookmarkManager *manager)
    : m_manager(manager)
{
    setId("CodeBookmark.Settings");
    setDisplayName(QCoreApplication::translate("CodeBookmark", "Code Bookmarks"));
    setCategory("B.Core");
    setWidgetCreator([this] { return new BookmarkOptionsPageWidget(m_manager); });
}

} // namespace Internal
} // namespace CodeBookmark

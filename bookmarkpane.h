// Copyright (C) 2026 CodeBookmark contributors.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <coreplugin/ioutputpane.h>

#include <QModelIndex>

class QTreeWidget;
class QTreeWidgetItem;

namespace CodeBookmark {

class BookmarkManager;

namespace Internal {

class BookmarkPane : public Core::IOutputPane
{
    Q_OBJECT

public:
    explicit BookmarkPane(BookmarkManager *manager, QObject *parent = nullptr);

    QWidget *outputWidget(QWidget *parent) override;
    QList<QWidget *> toolBarWidgets() const override;

    void clearContents() override;
    void setFocus() override;
    bool hasFocus() const override;
    bool canFocus() const override;

    bool canNavigate() const override;
    bool canNext() const override;
    bool canPrevious() const override;
    void goToNext() override;
    void goToPrev() override;

private:
    void refresh();
    void openEntry(QTreeWidgetItem *item);
    void openMarkdownFile();
    void onItemChanged(QTreeWidgetItem *item, int column);
    void showContextMenu(const QPoint &pos);
    void deleteSelectedBookmark();
    void clearAllBookmarks();

    BookmarkManager *m_manager;
    QTreeWidget *m_tree = nullptr;
    QWidget *m_toolBarWidget = nullptr;
    mutable bool m_refreshed = false;
    bool m_refreshing = false;
};

} // namespace Internal
} // namespace CodeBookmark

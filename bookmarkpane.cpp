// Copyright (C) 2026 CodeBookmark contributors.
// SPDX-License-Identifier: GPL-3.0-only

#include "bookmarkpane.h"

#include "bookmarkmanager.h"

#include <coreplugin/editormanager/editormanager.h>
#include <utils/link.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace Core;
using namespace Utils;

namespace CodeBookmark {
namespace Internal {

static const char kPaneId[] = "CodeBookmark.BookmarkPane";

// Delegate that only allows editing the Note column (column 5)
class NoteColumnDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        if (index.column() != 5)
            return nullptr;
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
};

BookmarkPane::BookmarkPane(BookmarkManager *manager, QObject *parent)
    : IOutputPane(parent)
    , m_manager(manager)
{
    setId(kPaneId);
    setDisplayName(tr("Code Bookmarks"));

    connect(m_manager, &BookmarkManager::bookmarksChanged, this, &BookmarkPane::refresh);
}

QWidget *BookmarkPane::outputWidget(QWidget *parent)
{
    if (m_tree)
        return m_tree;

    m_tree = new QTreeWidget(parent);
    m_tree->setColumnCount(6);
    m_tree->setHeaderLabels({tr("序号"), tr("时间"), tr("内容"), tr("文件"), tr("行号"), tr("备注")});
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setSortingEnabled(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_tree->setColumnWidth(0, 50);
    m_tree->setColumnWidth(1, 150);
    m_tree->setColumnWidth(2, 300);
    m_tree->setColumnWidth(3, 250);
    m_tree->setColumnWidth(4, 80);
    m_tree->setColumnWidth(5, 200);

    // Only Note column (column 5) is editable
    m_tree->setItemDelegate(new NoteColumnDelegate(m_tree));

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
            &BookmarkPane::showContextMenu);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *item) { openEntry(item); });
    connect(m_tree, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem *item, int column) { onItemChanged(item, column); });

    refresh();
    return m_tree;
}

QList<QWidget *> BookmarkPane::toolBarWidgets() const
{
    if (m_toolBarWidget)
        return {m_toolBarWidget};

    auto *widget = new QWidget;
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *openFileButton = new QToolButton(widget);
    openFileButton->setText(tr("Open MD"));
    openFileButton->setToolTip(tr("Open the bookmarks Markdown file"));
    openFileButton->setAutoRaise(true);
    QObject::connect(openFileButton, &QToolButton::clicked, this,
                     &BookmarkPane::openMarkdownFile);
    layout->addWidget(openFileButton);

    auto *refreshButton = new QToolButton(widget);
    refreshButton->setText(tr("Refresh"));
    refreshButton->setToolTip(tr("Reload bookmarks from disk"));
    refreshButton->setAutoRaise(true);
    QObject::connect(refreshButton, &QToolButton::clicked, this, [this] {
        m_manager->reload();
        const_cast<BookmarkPane *>(this)->refresh();
    });
    layout->addWidget(refreshButton);

    layout->addStretch();

    const_cast<BookmarkPane *>(this)->m_toolBarWidget = widget;
    return {widget};
}

void BookmarkPane::clearContents()
{
    if (m_tree)
        m_tree->clear();
}

void BookmarkPane::setFocus()
{
    if (m_tree)
        m_tree->setFocus();
}

bool BookmarkPane::hasFocus() const
{
    return m_tree && m_tree->hasFocus();
}

bool BookmarkPane::canFocus() const
{
    return true;
}

bool BookmarkPane::canNavigate() const
{
    return m_tree && m_tree->topLevelItemCount() > 1;
}

bool BookmarkPane::canNext() const
{
    return canNavigate();
}

bool BookmarkPane::canPrevious() const
{
    return canNavigate();
}

void BookmarkPane::goToNext()
{
    if (!m_tree)
        return;
    const int count = m_tree->topLevelItemCount();
    if (count == 0)
        return;
    const int current = m_tree->indexOfTopLevelItem(m_tree->currentItem());
    const int next = (current + 1) % count;
    m_tree->setCurrentItem(m_tree->topLevelItem(next));
}

void BookmarkPane::goToPrev()
{
    if (!m_tree)
        return;
    const int count = m_tree->topLevelItemCount();
    if (count == 0)
        return;
    const int current = m_tree->indexOfTopLevelItem(m_tree->currentItem());
    const int prev = (current - 1 + count) % count;
    m_tree->setCurrentItem(m_tree->topLevelItem(prev));
}

void BookmarkPane::refresh()
{
    if (!m_tree)
        return;

    m_refreshing = true;
    m_tree->setUpdatesEnabled(false);
    m_tree->clear();

    const QList<BookmarkEntry> &entries = m_manager->entries();
    int index = 1;
    for (const BookmarkEntry &entry : entries) {
        auto *item = new QTreeWidgetItem;
        // Column 0: 序号
        item->setText(0, QString::number(index));
        // Column 1: 时间
        item->setText(1, entry.timestamp);
        // Column 2: 标题
        item->setText(2, entry.title);
        item->setToolTip(2, entry.title);
        // Column 3: 文件
        item->setText(3, entry.filePath.toUserOutput());
        item->setToolTip(3, entry.filePath.toUserOutput());
        // Column 4: 行号
        item->setText(4, entry.startLine == entry.endLine
                             ? QString::number(entry.startLine)
                             : QStringLiteral("%1-%2").arg(entry.startLine).arg(entry.endLine));
        // Column 5: 备注
        item->setText(5, entry.note);
        item->setToolTip(5, entry.note);
        // Store data for navigation and editing
        item->setData(0, Qt::UserRole, entry.startLine);
        item->setData(0, Qt::UserRole + 1, entry.filePath.toVariant());
        item->setData(0, Qt::UserRole + 2, entry.timestamp);
        item->setData(0, Qt::UserRole + 3, entry.title);
        // Make only Note column (column 5) editable
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_tree->addTopLevelItem(item);
        ++index;
    }

    m_tree->setUpdatesEnabled(true);
    m_refreshing = false;
}

void BookmarkPane::openEntry(QTreeWidgetItem *item)
{
    if (!item)
        return;
    const int line = item->data(0, Qt::UserRole).toInt();
    const FilePath path = FilePath::fromVariant(item->data(0, Qt::UserRole + 1));
    if (path.isEmpty())
        return;
    Link link(path, line);
    EditorManager::openEditorAt(link);
}

void BookmarkPane::openMarkdownFile()
{
    EditorManager::openEditor(m_manager->filePath());
}

void BookmarkPane::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_refreshing || !item || column != 5)
        return;

    const QString timestamp = item->data(0, Qt::UserRole + 2).toString();
    const QString title = item->data(0, Qt::UserRole + 3).toString();
    const QString newNote = item->text(5);

    if (timestamp.isEmpty() || title.isEmpty())
        return;

    m_manager->updateNoteByTimestampAndTitle(timestamp, title, newNote);
}

void BookmarkPane::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item)
        return;

    QMenu menu(m_tree);
    QAction *deleteAction = menu.addAction(tr("删除"));
    QAction *clearAction = menu.addAction(tr("全部清空"));

    QAction *selectedAction = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (selectedAction == deleteAction) {
        deleteSelectedBookmark();
    } else if (selectedAction == clearAction) {
        clearAllBookmarks();
    }
}

void BookmarkPane::deleteSelectedBookmark()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item)
        return;

    const QString timestamp = item->data(0, Qt::UserRole + 2).toString();
    const QString title = item->data(0, Qt::UserRole + 3).toString();

    if (timestamp.isEmpty() || title.isEmpty())
        return;

    m_manager->removeBookmark(timestamp, title);
}

void BookmarkPane::clearAllBookmarks()
{
    const auto ret = QMessageBox::question(m_tree,
                                           tr("确认清空"),
                                           tr("确定要清空所有收藏吗？此操作不可恢复。"),
                                           QMessageBox::Yes | QMessageBox::No,
                                           QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_manager->clearAllBookmarks();
    }
}

} // namespace Internal
} // namespace CodeBookmark

// Copyright (C) 2026 CodeBookmark contributors.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <utils/filepath.h>

#include <QObject>
#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE
class QFileSystemWatcher;
QT_END_NAMESPACE

namespace CodeBookmark {

struct BookmarkEntry
{
    QString timestamp;
    QString title;
    QString note;
    Utils::FilePath filePath;
    int startLine = 0;
    int endLine = 0;
    QString language;
    QString code;

    bool isValid() const { return !filePath.isEmpty() && startLine > 0; }
};

class BookmarkManager : public QObject
{
    Q_OBJECT

public:
    explicit BookmarkManager(QObject *parent = nullptr);

    Utils::FilePath filePath() const;
    void setFilePath(const Utils::FilePath &path);

    const QList<BookmarkEntry> &entries() const { return m_entries; }
    void reload();

    bool addBookmark(const BookmarkEntry &entry);
    bool updateNote(const BookmarkEntry *entry, const QString &note);
    bool updateNoteByTimestampAndTitle(const QString &timestamp,
                                       const QString &title,
                                       const QString &note);

    const BookmarkEntry *findDuplicate(const Utils::FilePath &filePath,
                                       int startLine,
                                       const QString &trimmedCode) const;

    bool removeBookmark(const QString &timestamp, const QString &title);
    void clearAllBookmarks();

    static QString languageForFile(const Utils::FilePath &filePath);
    static QString guessTitle(const QString &selectedText, const Utils::FilePath &filePath);

signals:
    void bookmarksChanged();

private:
    static Utils::FilePath defaultFilePath();
    QString renderEntry(const BookmarkEntry &entry) const;

    Utils::FilePath m_filePath;
    QList<BookmarkEntry> m_entries;
    class QFileSystemWatcher *m_watcher = nullptr;
    bool m_ignoreWatcherSignal = false;
};

} // namespace CodeBookmark

// Copyright (C) 2026 CodeBookmark contributors.
// SPDX-License-Identifier: GPL-3.0-only

#include "bookmarkmanager.h"

#include <coreplugin/icore.h>

#include <utils/qtcsettings.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QRegularExpression>
#include <QTextStream>

using namespace Core;
using namespace Utils;

namespace CodeBookmark {

static const char kSettingsGroup[] = "CodeBookmark";
static const char kSettingsFilePath[] = "MarkdownFilePath";

BookmarkManager::BookmarkManager(QObject *parent)
    : QObject(parent)
{
    m_filePath = defaultFilePath();

    QtcSettings *settings = ICore::settings();
    settings->beginGroup(kSettingsGroup);
    const QString stored = settings->value(kSettingsFilePath).toString();
    settings->endGroup();
    if (!stored.isEmpty())
        m_filePath = FilePath::fromString(stored);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        if (m_ignoreWatcherSignal)
            return;
        reload();
        emit bookmarksChanged();
    });

    reload();
}

FilePath BookmarkManager::filePath() const
{
    return m_filePath;
}

void BookmarkManager::setFilePath(const FilePath &path)
{
    if (m_filePath == path)
        return;

    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    m_filePath = path;

    QtcSettings *settings = ICore::settings();
    settings->beginGroup(kSettingsGroup);
    settings->setValue(kSettingsFilePath, path.toSettings());
    settings->endGroup();

    reload();
    emit bookmarksChanged();
}

FilePath BookmarkManager::defaultFilePath()
{
    return ICore::userResourcePath("codebookmarks.md");
}

void BookmarkManager::reload()
{
    m_entries.clear();

    QFile file(m_filePath.toString());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    // Parse the format written by renderEntry():
    //   ## [timestamp] title
    //   > note: ...
    //   **Location**: `path:start-end`
    //   ```lang
    //   code
    //   ```
    enum State { None, InNote, InCode };
    State state = None;
    BookmarkEntry current;
    QString noteText;
    QStringList codeLines;
    QString codeLang;
    static const QRegularExpression headerRe(
        QStringLiteral("^## \\[(.+?)\\] (.*)$"));
    static const QRegularExpression locationRe(
        QStringLiteral("^\\*\\*Location\\*\\*\\s*:\\s*`(.+?):(\\d+)(?:-(\\d+))?`$"));
    static const QRegularExpression fenceRe(QStringLiteral("^```(.*)$"));

    auto flushEntry = [this, &current, &noteText, &codeLines]() {
        if (!current.isValid())
            return;
        current.note = noteText.trimmed();
        current.code = codeLines.join('\n');
        m_entries.append(current);
        current = BookmarkEntry();
        noteText.clear();
        codeLines.clear();
    };

    while (!in.atEnd()) {
        const QString line = in.readLine();

        if (state == InCode) {
            if (line.startsWith("```")) {
                state = None;
                flushEntry();
                continue;
            }
            codeLines.append(line);
            continue;
        }

        const auto headerMatch = headerRe.match(line);
        if (headerMatch.hasMatch()) {
            flushEntry();
            current.timestamp = headerMatch.captured(1);
            current.title = headerMatch.captured(2);
            noteText.clear();
            codeLines.clear();
            continue;
        }

        if (line.startsWith("> ")) {
            if (line.startsWith("> note: ", Qt::CaseInsensitive))
                noteText += line.mid(8);
            else
                noteText += line.mid(2);
            noteText += '\n';
            state = InNote;
            continue;
        }

        const auto locationMatch = locationRe.match(line);
        if (locationMatch.hasMatch()) {
            current.filePath = FilePath::fromString(locationMatch.captured(1));
            current.startLine = locationMatch.captured(2).toInt();
            current.endLine = locationMatch.captured(3).isEmpty()
                                  ? current.startLine
                                  : locationMatch.captured(3).toInt();
            current.language = languageForFile(current.filePath);
            state = None;
            continue;
        }

        const auto fenceMatch = fenceRe.match(line);
        if (fenceMatch.hasMatch()) {
            codeLang = fenceMatch.captured(1);
            state = InCode;
            codeLines.clear();
            continue;
        }

        if (line.trimmed().isEmpty())
            state = None;
    }
    flushEntry();
}

bool BookmarkManager::addBookmark(const BookmarkEntry &entry)
{
    if (!entry.isValid())
        return false;

    QFile file(m_filePath.toString());
    const bool needHeader = !file.exists() || file.size() == 0;
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return false;

    m_ignoreWatcherSignal = true;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    if (needHeader) {
        out << "# Code Bookmarks\n\n";
        out << "> Generated by the CodeBookmark plugin for Qt Creator.\n\n";
    }

    out << renderEntry(entry);
    out.flush();
    file.close();
    m_ignoreWatcherSignal = false;

    reload();
    emit bookmarksChanged();
    return true;
}

bool BookmarkManager::updateNote(const BookmarkEntry *entry, const QString &note)
{
    if (!entry)
        return false;
    return updateNoteByTimestampAndTitle(entry->timestamp, entry->title, note);
}

bool BookmarkManager::updateNoteByTimestampAndTitle(const QString &timestamp,
                                                    const QString &title,
                                                    const QString &note)
{
    QFile file(m_filePath.toString());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QString content = in.readAll();
    file.close();

    // Find the block for this entry and replace the note line.
    // Block starts with "## [timestamp] title" and ends before the next "## [" or EOF.
    const QString header = QStringLiteral("## [%1] %2").arg(timestamp, title);
    const int headerPos = content.indexOf(header);
    if (headerPos < 0)
        return false;

    int blockEnd = content.indexOf(QStringLiteral("\n## ["), headerPos + 1);
    if (blockEnd < 0)
        blockEnd = content.length();

    QString block = content.mid(headerPos, blockEnd - headerPos);
    static const QRegularExpression noteRe(QStringLiteral("^> note: .*$"),
                                           QRegularExpression::MultilineOption);
    const QString newNoteLine = QStringLiteral("> note: ") + note;
    if (block.contains(noteRe))
        block.replace(noteRe, newNoteLine);
    else
        block.replace(header + '\n', header + '\n' + newNoteLine + '\n');

    QString newContent = content;
    newContent.replace(headerPos, blockEnd - headerPos, block);

    m_ignoreWatcherSignal = true;
    QFile outFile(m_filePath.toString());
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_ignoreWatcherSignal = false;
        return false;
    }
    QTextStream out(&outFile);
    out.setEncoding(QStringConverter::Utf8);
    out << newContent;
    out.flush();
    outFile.close();
    m_ignoreWatcherSignal = false;

    reload();
    emit bookmarksChanged();
    return true;
}

const BookmarkEntry *BookmarkManager::findDuplicate(const FilePath &filePath,
                                                    int startLine,
                                                    const QString &trimmedCode) const
{
    for (const BookmarkEntry &entry : m_entries) {
        if (entry.filePath == filePath && entry.startLine == startLine
            && entry.code.trimmed() == trimmedCode) {
            return &entry;
        }
    }
    return nullptr;
}

QString BookmarkManager::languageForFile(const FilePath &filePath)
{
    const QString suffix = filePath.suffix().toLower();
    static const QHash<QString, QString> map = {
        {"c", "c"},         {"cc", "cpp"},       {"cpp", "cpp"},
        {"cxx", "cpp"},     {"h", "cpp"},        {"hh", "cpp"},
        {"hpp", "cpp"},     {"hxx", "cpp"},      {"m", "objective-c"},
        {"mm", "objective-cpp"}, {"cs", "csharp"}, {"java", "java"},
        {"py", "python"},   {"js", "javascript"}, {"ts", "typescript"},
        {"tsx", "typescript"}, {"jsx", "jsx"},   {"go", "go"},
        {"rs", "rust"},     {"rb", "ruby"},      {"php", "php"},
        {"swift", "swift"}, {"kt", "kotlin"},    {"kts", "kotlin"},
        {"scala", "scala"}, {"lua", "lua"},      {"sh", "bash"},
        {"bash", "bash"},   {"zsh", "bash"},     {"ps1", "powershell"},
        {"qml", "qml"},     {"js", "javascript"}, {"json", "json"},
        {"xml", "xml"},     {"html", "html"},    {"htm", "html"},
        {"css", "css"},     {"scss", "scss"},    {"sass", "sass"},
        {"sql", "sql"},     {"yaml", "yaml"},    {"yml", "yaml"},
        {"toml", "toml"},   {"ini", "ini"},      {"cmake", "cmake"},
        {"mk", "makefile"}, {"mak", "makefile"}, {"pro", "qmake"},
        {"pri", "qmake"},   {"md", "markdown"},  {"tex", "latex"},
        {"rst", "rst"},     {"dart", "dart"},    {"r", "r"},
        {"pl", "perl"},     {"ex", "elixir"},    {"exs", "elixir"},
        {"hs", "haskell"},  {"clj", "clojure"},  {"vim", "vim"},
    };
    const QString lang = map.value(suffix);
    if (!lang.isEmpty())
        return lang;

    const QString fileName = filePath.fileName().toLower();
    if (fileName == "cmakelists.txt")
        return "cmake";
    if (fileName == "makefile")
        return "makefile";
    if (fileName == "dockerfile")
        return "dockerfile";
    if (fileName.endsWith(".pro") || fileName.endsWith(".pri"))
        return "qmake";
    return {};
}

QString BookmarkManager::guessTitle(const QString &selectedText, const FilePath &filePath)
{
    // Heuristic: find a function/method definition line inside the selection.
    static const QRegularExpression funcRe(
        QStringLiteral("\\b([A-Za-z_][A-Za-z0-9_:~]*)\\s*\\([^;]*\\)\\s*(const)?\\s*"
                       "(->\\s*[^{;]+)?\\s*\\{"));
    const QStringList lines = selectedText.split('\n');
    for (const QString &line : lines) {
        const auto match = funcRe.match(line.trimmed());
        if (match.hasMatch())
            return match.captured(1);
    }

    // Fallback: class/struct/namespace keyword on the first non-empty line.
    static const QRegularExpression typeRe(
        QStringLiteral("\\b(class|struct|namespace)\\s+([A-Za-z_][A-Za-z0-9_]*)"));
    for (const QString &line : lines) {
        const auto match = typeRe.match(line.trimmed());
        if (match.hasMatch())
            return match.captured(1) + QStringLiteral("::") + match.captured(2);
    }

    return filePath.completeBaseName();
}

bool BookmarkManager::removeBookmark(const QString &timestamp, const QString &title)
{
    QFile file(m_filePath.toString());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QString content = in.readAll();
    file.close();

    const QString header = QStringLiteral("## [%1] %2").arg(timestamp, title);
    const int headerPos = content.indexOf(header);
    if (headerPos < 0)
        return false;

    int blockEnd = content.indexOf(QStringLiteral("\n## ["), headerPos + 1);
    if (blockEnd < 0)
        blockEnd = content.length();

    QString newContent = content;
    newContent.remove(headerPos, blockEnd - headerPos);

    // Clean up extra blank lines
    newContent.replace(QRegularExpression(QStringLiteral("\n{3,}")), QStringLiteral("\n\n"));

    m_ignoreWatcherSignal = true;
    QFile outFile(m_filePath.toString());
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_ignoreWatcherSignal = false;
        return false;
    }
    QTextStream out(&outFile);
    out.setEncoding(QStringConverter::Utf8);
    out << newContent;
    out.flush();
    outFile.close();
    m_ignoreWatcherSignal = false;

    reload();
    emit bookmarksChanged();
    return true;
}

void BookmarkManager::clearAllBookmarks()
{
    m_ignoreWatcherSignal = true;
    QFile file(m_filePath.toString());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        file.close();
    }
    m_ignoreWatcherSignal = false;

    reload();
    emit bookmarksChanged();
}

QString BookmarkManager::renderEntry(const BookmarkEntry &entry) const
{
    QString md;
    md += QStringLiteral("## [%1] %2\n\n").arg(entry.timestamp, entry.title);
    if (!entry.note.isEmpty())
        md += QStringLiteral("> note: %1\n\n").arg(entry.note);
    md += QStringLiteral("**Location**: `%1:%2-%3`\n\n")
              .arg(entry.filePath.toUserOutput())
              .arg(entry.startLine)
              .arg(entry.endLine);
    const QString lang = entry.language.isEmpty() ? QString() : entry.language;
    md += QStringLiteral("```%1\n%2\n```\n\n").arg(lang, entry.code);
    return md;
}

} // namespace CodeBookmark

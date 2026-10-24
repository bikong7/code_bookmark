// Copyright (C) 2026 CodeBookmark contributors.
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <coreplugin/dialogs/ioptionspage.h>

namespace CodeBookmark {

class BookmarkManager;

namespace Internal {

class BookmarkOptionsPageWidget;

class BookmarkOptionsPage : public Core::IOptionsPage
{
    Q_DISABLE_COPY_MOVE(BookmarkOptionsPage)

public:
    explicit BookmarkOptionsPage(BookmarkManager *manager);

private:
    BookmarkManager *m_manager;
};

} // namespace Internal
} // namespace CodeBookmark

// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "clangutils.h"

#include <languageserverprotocol/lsptypes.h>

#include <texteditor/textmark.h>

#include <QPointer>

#include <functional>

namespace LanguageClient { class Client; }

namespace ClangCodeModel {
namespace Internal {

class ClangdClient;

class ClangdTextMark : public TextEditor::TextMark
{
    Q_DECLARE_TR_FUNCTIONS(ClangdTextMark)
public:
    ClangdTextMark(const ::Utils::FilePath &filePath,
                   const LanguageServerProtocol::Diagnostic &diagnostic,
                   bool isProjectFile,
                   ClangdClient *client);

private:
    bool addToolTipContent(QLayout *target) const override;

    const LanguageServerProtocol::Diagnostic m_lspDiagnostic;
    const ClangDiagnostic m_diagnostic;
    const QPointer<const LanguageClient::Client> m_client;
};

} // namespace Internal
} // namespace ClangCodeModel

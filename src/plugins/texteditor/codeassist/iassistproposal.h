// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "assistenums.h"
#include "iassistproposalmodel.h"

#include <texteditor/texteditor_global.h>

#include <utils/id.h>

namespace TextEditor {

class IAssistProposalWidget;
class TextEditorWidget;

class TEXTEDITOR_EXPORT IAssistProposal
{
public:
    IAssistProposal(Utils::Id id,int basePosition);
    virtual ~IAssistProposal();

    int basePosition() const;
    bool isFragile() const;
    bool supportsPrefixFiltering(const QString &prefix) const;
    virtual bool hasItemsToPropose(const QString &, AssistReason) const { return true; }
    virtual bool isCorrective(TextEditorWidget *editorWidget) const;
    virtual void makeCorrection(TextEditorWidget *editorWidget);
    virtual TextEditor::ProposalModelPtr model() const = 0;
    virtual IAssistProposalWidget *createWidget() const = 0;

    void setFragile(bool fragile);

    Utils::Id id() const { return m_id; }

    AssistReason reason() const { return m_reason; }
    void setReason(const AssistReason &reason) { m_reason = reason; }

    using PrefixChecker = std::function<bool(const QString &)>;
    void setPrefixChecker(const PrefixChecker checker);

protected:
    Utils::Id m_id;
    int m_basePosition;
    bool m_isFragile = false;
    PrefixChecker m_prefixChecker;
    AssistReason m_reason = IdleEditor;
};

} // TextEditor

// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/ioutlinewidget.h>

#include <QSortFilterProxyModel>

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

namespace Core { class IEditor; }

namespace QmlJS { class Editor; }

namespace QmlJSEditor {

class QmlJSEditorWidget;

namespace Internal {

class QmlJSOutlineTreeView;

class QmlJSOutlineFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    QmlJSOutlineFilterModel(QObject *parent);
    // QSortFilterProxyModel
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::DropActions supportedDragActions() const override;

    bool filterBindings() const;
    void setFilterBindings(bool filterBindings);
    void setSorted(bool sorted);
private:
    bool m_filterBindings = false;
    bool m_sorted = false;
};

class QmlJSOutlineWidget : public TextEditor::IOutlineWidget
{
    Q_OBJECT
public:
    QmlJSOutlineWidget(QWidget *parent = nullptr);

    void setEditor(QmlJSEditorWidget *editor);

    // IOutlineWidget
    QList<QAction*> filterMenuActions() const override;
    void setCursorSynchronization(bool syncWithCursor) override;
    bool isSorted() const override { return m_sorted; };
    void setSorted(bool sorted) override;
    void restoreSettings(const QVariantMap &map) override;
    QVariantMap settings() const override;

private:
    void updateSelectionInTree(const QModelIndex &index);
    void updateSelectionInText(const QItemSelection &selection);
    void updateTextCursor(const QModelIndex &index);
    void focusEditor();
    void setShowBindings(bool showBindings);
    bool syncCursor();

private:
    QmlJSOutlineTreeView *m_treeView = nullptr;
    QmlJSOutlineFilterModel *m_filterModel = nullptr;
    QmlJSEditorWidget *m_editor = nullptr;

    QAction *m_showBindingsAction = nullptr;

    bool m_enableCursorSync = true;
    bool m_blockCursorSync = false;
    bool m_sorted = false;
};

class QmlJSOutlineWidgetFactory : public TextEditor::IOutlineWidgetFactory
{
    Q_OBJECT
public:
    bool supportsEditor(Core::IEditor *editor) const override;
    bool supportsSorting() const override { return true; }
    TextEditor::IOutlineWidget *createWidget(Core::IEditor *editor) override;
};

} // namespace Internal
} // namespace QmlJSEditor

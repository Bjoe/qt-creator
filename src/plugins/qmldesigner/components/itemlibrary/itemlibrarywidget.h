// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "itemlibraryinfo.h"
#include "import.h"

#include <utils/fancylineedit.h>
#include <utils/dropsupport.h>
#include <previewtooltip/previewtooltipbackend.h>

#include <QFileIconProvider>
#include <QFrame>
#include <QPointF>
#include <QQmlPropertyMap>
#include <QQuickWidget>
#include <QTimer>
#include <QToolButton>

#include <memory>

QT_BEGIN_NAMESPACE
class QStackedWidget;
class QShortcut;
QT_END_NAMESPACE

namespace QmlDesigner {

class MetaInfo;
class ItemLibraryEntry;
class Model;

class ItemLibraryModel;
class ItemLibraryAddImportModel;
class ItemLibraryResourceView;
class AsynchronousImageCache;
class ImageCacheCollector;

class ItemLibraryWidget : public QFrame
{
    Q_OBJECT

public:
    Q_PROPERTY(bool subCompEditMode READ subCompEditMode NOTIFY subCompEditModeChanged)

    ItemLibraryWidget(AsynchronousImageCache &imageCache);
    ~ItemLibraryWidget();

    void setItemLibraryInfo(ItemLibraryInfo *itemLibraryInfo);
    QList<QToolButton *> createToolBarWidgets();

    static QString qmlSourcesPath();

    void clearSearchFilter();
    void switchToComponentsView();
    void delayedUpdateModel();
    void updateModel();
    void updatePossibleImports(const QList<Import> &possibleImports);
    void updateUsedImports(const QList<Import> &usedImports);

    void setModel(Model *model);
    void setFlowMode(bool b);

    inline static bool isHorizontalLayout = false;

    bool subCompEditMode() const;

    Q_INVOKABLE void startDragAndDrop(const QVariant &itemLibEntry, const QPointF &mousePos);
    Q_INVOKABLE void removeImport(const QString &importUrl);
    Q_INVOKABLE void addImportForItem(const QString &importUrl);
    Q_INVOKABLE void handleSearchFilterChanged(const QString &filterText);
    Q_INVOKABLE void handleAddImport(int index);
    Q_INVOKABLE void goIntoComponent(const QString &source);

signals:
    void itemActivated(const QString &itemName);
    void subCompEditModeChanged();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void reloadQmlSource();

    void updateSearch();
    void handlePriorityImportsChanged();
    static QString getDependencyImport(const Import &import);

    QTimer m_compressionTimer;
    QSize m_itemIconSize;

    QPointer<ItemLibraryInfo> m_itemLibraryInfo;

    QPointer<ItemLibraryModel> m_itemLibraryModel;
    QPointer<ItemLibraryAddImportModel> m_addModuleModel;

    QScopedPointer<QQuickWidget> m_itemsWidget;
    std::unique_ptr<PreviewTooltipBackend> m_previewTooltipBackend;

    QShortcut *m_qmlSourceUpdateShortcut;
    AsynchronousImageCache &m_imageCache;
    QPointer<Model> m_model;
    QVariant m_itemToDrag;
    bool m_updateRetry = false;
    QString m_filterText;
    QPoint m_dragStartPoint;
    bool m_subCompEditMode = false;

    inline static int HORIZONTAL_LAYOUT_WIDTH_LIMIT = 600;
};

} // namespace QmlDesigner

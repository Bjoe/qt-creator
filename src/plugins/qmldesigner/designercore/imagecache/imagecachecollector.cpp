// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "imagecachecollector.h"
#include "imagecacheconnectionmanager.h"

#include <metainfo.h>
#include <model.h>
#include <nodeinstanceview.h>
#include <plaintexteditmodifier.h>
#include <rewriterview.h>

#include <projectexplorer/project.h>
#include <projectexplorer/target.h>
#include <utils/fileutils.h>

#include <QPlainTextEdit>

namespace QmlDesigner {

namespace {

QByteArray fileToByteArray(QString const &filename)
{
    QFile file(filename);
    QFileInfo fleInfo(file);

    if (fleInfo.exists() && file.open(QFile::ReadOnly))
        return file.readAll();

    return {};
}

QString fileToString(const QString &filename)
{
    return QString::fromUtf8(fileToByteArray(filename));
}

} // namespace

ImageCacheCollector::ImageCacheCollector(ImageCacheConnectionManager &connectionManager,
                                         QSize captureImageMinimumSize,
                                         QSize captureImageMaximumSize,
                                         ExternalDependenciesInterface &externalDependencies,
                                         ImageCacheCollectorNullImageHandling nullImageHandling)
    : m_connectionManager{connectionManager}
    , captureImageMinimumSize{captureImageMinimumSize}
    , captureImageMaximumSize{captureImageMaximumSize}
    , m_externalDependencies{externalDependencies}
    , nullImageHandling{nullImageHandling}
{}

ImageCacheCollector::~ImageCacheCollector() = default;

void ImageCacheCollector::start(Utils::SmallStringView name,
                                Utils::SmallStringView state,
                                const ImageCache::AuxiliaryData &auxiliaryData,
                                CaptureCallback captureCallback,
                                AbortCallback abortCallback)
{
    RewriterView rewriterView{m_externalDependencies, RewriterView::Amend};
    NodeInstanceView nodeInstanceView{m_connectionManager, m_externalDependencies};
    nodeInstanceView.setCaptureImageMinimumAndMaximumSize(captureImageMinimumSize,
                                                          captureImageMaximumSize);

    const QString filePath{name};
    auto model = QmlDesigner::Model::create("QtQuick/Item", 2, 1);
    model->setFileUrl(QUrl::fromLocalFile(filePath));

    auto textDocument = std::make_unique<QTextDocument>(fileToString(filePath));

    auto modifier = std::make_unique<NotIndentingTextEditModifier>(textDocument.get(),
                                                                   QTextCursor{textDocument.get()});

    rewriterView.setTextModifier(modifier.get());

    model->setRewriterView(&rewriterView);

    auto rootModelNodeMetaInfo = rewriterView.rootModelNode().metaInfo();
    bool is3DRoot = rewriterView.errors().isEmpty()
                    && (rootModelNodeMetaInfo.isQtQuick3DNode()
                        || rootModelNodeMetaInfo.isQtQuick3DMaterial());

    if (!rewriterView.errors().isEmpty() || (!rewriterView.rootModelNode().metaInfo().isGraphicalItem()
                                        && !is3DRoot)) {
        if (abortCallback)
            abortCallback(ImageCache::AbortReason::Failed);
        return;
    }

    if (is3DRoot) {
        if (auto libIcon = std::get_if<ImageCache::LibraryIconAuxiliaryData>(&auxiliaryData))
            rewriterView.rootModelNode().setAuxiliaryData(AuxiliaryDataType::NodeInstancePropertyOverwrite,
                                                          "isLibraryIcon",
                                                          libIcon->enable);
    }

    ModelNode stateNode = rewriterView.modelNodeForId(QString{state});

    if (stateNode.isValid())
        rewriterView.setCurrentStateNode(stateNode);

    auto callback = [=, captureCallback = std::move(captureCallback)](const QImage &image) {
        if (nullImageHandling == ImageCacheCollectorNullImageHandling::CaptureNullImage
            || !image.isNull()) {
            QSize smallImageSize = image.size().scaled(QSize{96, 96}.boundedTo(image.size()),
                                                       Qt::KeepAspectRatio);
            QImage smallImage = image.isNull() ? QImage{}
                                               : image.scaled(smallImageSize,
                                                              Qt::IgnoreAspectRatio,
                                                              Qt::SmoothTransformation);
            captureCallback(image, smallImage);
        }
    };

    if (!m_target)
        return;

    nodeInstanceView.setTarget(m_target.data());
    m_connectionManager.setCallback(std::move(callback));
    nodeInstanceView.setCrashCallback([=] { abortCallback(ImageCache::AbortReason::Failed); });
    model->setNodeInstanceView(&nodeInstanceView);

    bool capturedDataArrived = m_connectionManager.waitForCapturedData();

    m_connectionManager.setCallback({});
    m_connectionManager.setCrashCallback({});

    model->setNodeInstanceView({});
    model->setRewriterView({});

    if (!capturedDataArrived && abortCallback)
        abortCallback(ImageCache::AbortReason::Failed);
}

std::pair<QImage, QImage> ImageCacheCollector::createImage(Utils::SmallStringView,
                                                           Utils::SmallStringView,
                                                           const ImageCache::AuxiliaryData &)
{
    return {};
}

QIcon ImageCacheCollector::createIcon(Utils::SmallStringView,
                                      Utils::SmallStringView,
                                      const ImageCache::AuxiliaryData &)
{
    return {};
}

void ImageCacheCollector::setTarget(ProjectExplorer::Target *target)
{
    m_target = target;
}

ProjectExplorer::Target *ImageCacheCollector::target() const
{
    return m_target;
}

} // namespace QmlDesigner

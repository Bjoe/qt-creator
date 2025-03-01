// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "submitfilemodel.h"

#include <utils/fsengine/fileiconprovider.h>
#include <utils/qtcassert.h>
#include <utils/theme/theme.h>

#include <QStandardItem>
#include <QDebug>

using namespace Utils;

namespace VcsBase {

// --------------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------------

enum { StateColumn = 0, FileColumn = 1 };

static QBrush fileStatusTextForeground(SubmitFileModel::FileStatusHint statusHint)
{
    Theme::Color statusTextColor = Theme::VcsBase_FileStatusUnknown_TextColor;
    switch (statusHint) {
    case SubmitFileModel::FileStatusUnknown:
        statusTextColor = Theme::VcsBase_FileStatusUnknown_TextColor;
        break;
    case SubmitFileModel::FileAdded:
        statusTextColor = Theme::VcsBase_FileAdded_TextColor;
        break;
    case SubmitFileModel::FileModified:
        statusTextColor = Theme::VcsBase_FileModified_TextColor;
        break;
    case SubmitFileModel::FileDeleted:
        statusTextColor = Theme::VcsBase_FileDeleted_TextColor;
        break;
    case SubmitFileModel::FileRenamed:
        statusTextColor = Theme::VcsBase_FileRenamed_TextColor;
        break;
    case SubmitFileModel::FileUnmerged:
        statusTextColor = Theme::VcsBase_FileUnmerged_TextColor;
        break;
    }
    return QBrush(Utils::creatorTheme()->color(statusTextColor));
}

static QList<QStandardItem *> createFileRow(const FilePath &repositoryRoot,
                                            const QString &fileName,
                                            const QString &status,
                                            SubmitFileModel::FileStatusHint statusHint,
                                            CheckMode checked,
                                            const QVariant &v)
{
    auto statusItem = new QStandardItem(status);
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (checked != Uncheckable) {
        flags |= Qt::ItemIsUserCheckable;
        statusItem->setCheckState(checked == Checked ? Qt::Checked : Qt::Unchecked);
    }
    statusItem->setFlags(flags);
    statusItem->setData(v);
    auto fileItem = new QStandardItem(fileName);
    fileItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    // For some reason, Windows (at least) requires a valid (existing) file path to the icon, so
    // the repository root is needed here.
    // Note: for "overlaid" icons in Utils::FileIconProvider a valid file path is not required
    fileItem->setIcon(FileIconProvider::icon(repositoryRoot.pathAppended(fileName)));
    const QList<QStandardItem *> row{statusItem, fileItem};
    if (statusHint != SubmitFileModel::FileStatusUnknown) {
        const QBrush textForeground = fileStatusTextForeground(statusHint);
        for (QStandardItem *item : row)
            item->setForeground(textForeground);
    }
    return row;
}

// --------------------------------------------------------------------------
// SubmitFileModel:
// --------------------------------------------------------------------------

/*!
    \class VcsBase::SubmitFileModel

    \brief The SubmitFileModel class is a 2-column (checkable, state, file name)
    model to be used to list the files in the submit editor.

    Provides header items and a convenience function to add files.
 */

SubmitFileModel::SubmitFileModel(QObject *parent) :
    QStandardItemModel(0, 2, parent)
{
    setHorizontalHeaderLabels({tr("State"), tr("File")});
}

const FilePath &SubmitFileModel::repositoryRoot() const
{
    return m_repositoryRoot;
}

void SubmitFileModel::setRepositoryRoot(const FilePath &repoRoot)
{
    m_repositoryRoot = repoRoot;
}

QList<QStandardItem *> SubmitFileModel::addFile(const QString &fileName, const QString &status, CheckMode checkMode,
                                                const QVariant &v)
{
    const FileStatusHint statusHint =
            m_fileStatusQualifier ? m_fileStatusQualifier(status, v) : FileStatusUnknown;
    const QList<QStandardItem *> row =
            createFileRow(m_repositoryRoot, fileName, status, statusHint, checkMode, v);
    appendRow(row);
    return row;
}

QString SubmitFileModel::state(int row) const
{
    if (row < 0 || row >= rowCount())
        return QString();
    return item(row)->text();
}

QString SubmitFileModel::file(int row) const
{
    if (row < 0 || row >= rowCount())
        return QString();
    return item(row, FileColumn)->text();
}

bool SubmitFileModel::isCheckable(int row) const
{
    if (row < 0 || row >= rowCount())
        return false;
    return item(row)->isCheckable();
}

bool SubmitFileModel::checked(int row) const
{
    if (row < 0 || row >= rowCount())
        return false;
    return (item(row)->checkState() == Qt::Checked);
}

void SubmitFileModel::setChecked(int row, bool check)
{
    if (row >= 0 || row < rowCount())
        item(row)->setCheckState(check ? Qt::Checked : Qt::Unchecked);
}

void SubmitFileModel::setAllChecked(bool check)
{
    int rows = rowCount();
    for (int row = 0; row < rows; ++row) {
        QStandardItem *i = item(row);
        if (i->isCheckable())
            i->setCheckState(check ? Qt::Checked : Qt::Unchecked);
    }
}

QVariant SubmitFileModel::extraData(int row) const
{
    if (row < 0 || row >= rowCount())
        return false;
    return item(row)->data();
}

bool SubmitFileModel::hasCheckedFiles() const
{
    for (int i = 0; i < rowCount(); ++i) {
        if (checked(i))
            return true;
    }
    return false;
}

unsigned int SubmitFileModel::filterFiles(const QStringList &filter)
{
    unsigned int rc = 0;
    for (int r = rowCount() - 1; r >= 0; r--)
        if (!filter.contains(file(r))) {
            removeRow(r);
            rc++;
        }
    return rc;
}

/*! Updates user selections from \a source model.
 *
 *  Assumes that both models are sorted with the same order, and there
 *              are no duplicate entries.
 */
void SubmitFileModel::updateSelections(SubmitFileModel *source)
{
    QTC_ASSERT(source, return);
    int rows = rowCount();
    int sourceRows = source->rowCount();
    int lastMatched = 0;
    for (int i = 0; i < rows; ++i) {
        // Since both models are sorted with the same order, there is no need
        // to test rows earlier than latest match found
        for (int j = lastMatched; j < sourceRows; ++j) {
            if (file(i) == source->file(j) && state(i) == source->state(j)) {
                if (isCheckable(i) && source->isCheckable(j))
                    setChecked(i, source->checked(j));
                lastMatched = j + 1; // No duplicates, start on next entry
                break;
            }
        }
    }
}

const SubmitFileModel::FileStatusQualifier &SubmitFileModel::fileStatusQualifier() const
{
    return m_fileStatusQualifier;
}

void SubmitFileModel::setFileStatusQualifier(FileStatusQualifier &&func)
{
    const int topLevelRowCount = rowCount();
    const int topLevelColCount = columnCount();
    for (int row = 0; row < topLevelRowCount; ++row) {
        const QStandardItem *statusItem = item(row, StateColumn);
        const FileStatusHint statusHint =
                func ? func(statusItem->text(), statusItem->data()) : FileStatusUnknown;
        const QBrush textForeground = fileStatusTextForeground(statusHint);
        for (int col = 0; col < topLevelColCount; ++col)
            item(row, col)->setForeground(textForeground);
    }
    m_fileStatusQualifier = func;
}

} // namespace VcsBase

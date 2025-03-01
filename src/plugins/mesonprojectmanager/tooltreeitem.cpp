// Copyright (C) 2020 Alexis Jeandet.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "tooltreeitem.h"

#include "mesonprojectmanagertr.h"

#include <utils/utilsicons.h>

#include <QFileInfo>
#include <QUuid>

namespace MesonProjectManager {
namespace Internal {

ToolTreeItem::ToolTreeItem(const QString &name)
    : m_name{name}
    , m_autoDetected{false}
    , m_id(Utils::Id::fromString(QUuid::createUuid().toString()))
    , m_unsavedChanges{true}
{
    self_check();
    update_tooltip();
}

ToolTreeItem::ToolTreeItem(const MesonTools::Tool_t &tool)
    : m_name{tool->name()}
    , m_executable{tool->exe()}
    , m_autoDetected{tool->autoDetected()}
    , m_id{tool->id()}
{
    m_tooltip = Tr::tr("Version: %1").arg(tool->version().toQString());
    self_check();
}

ToolTreeItem::ToolTreeItem(const ToolTreeItem &other)
    : m_name{Tr::tr("Clone of %1").arg(other.m_name)}
    , m_executable{other.m_executable}
    , m_autoDetected{false}
    , m_id{Utils::Id::fromString(QUuid::createUuid().toString())}
    , m_unsavedChanges{true}
{
    self_check();
    update_tooltip();
}

QVariant ToolTreeItem::data(int column, int role) const
{
    switch (role) {
    case Qt::DisplayRole: {
        switch (column) {
        case 0: {
            return m_name;
        }
        case 1: {
            return m_executable.toUserOutput();
        }
        } // switch (column)
        return QVariant();
    }
    case Qt::FontRole: {
        QFont font;
        font.setBold(m_unsavedChanges);
        return font;
    }
    case Qt::ToolTipRole: {
        if (!m_pathExists)
            return Tr::tr("Meson executable path does not exist.");
        if (!m_pathIsFile)
            return Tr::tr("Meson executable path is not a file.");
        if (!m_pathIsExecutable)
            return Tr::tr("Meson executable path is not executable.");
        return m_tooltip;
    }
    case Qt::DecorationRole: {
        if (column == 0 && (!m_pathExists || !m_pathIsFile || !m_pathIsExecutable))
            return Utils::Icons::CRITICAL.icon();
        return {};
    }
    }
    return {};
}

void ToolTreeItem::update(const QString &name, const Utils::FilePath &exe)
{
    m_unsavedChanges = true;
    m_name = name;
    if (exe != m_executable) {
        m_executable = exe;
        self_check();
        update_tooltip();
    }
}

void ToolTreeItem::self_check()
{
    m_pathExists = m_executable.exists();
    m_pathIsFile = m_executable.toFileInfo().isFile();
    m_pathIsExecutable = m_executable.toFileInfo().isExecutable();
}

void ToolTreeItem::update_tooltip(const Version &version)
{
    if (version.isValid)
        m_tooltip = Tr::tr("Version: %1").arg(version.toQString());
    else
        m_tooltip = Tr::tr("Cannot get tool version.");
}

void ToolTreeItem::update_tooltip()
{
    update_tooltip(MesonWrapper::read_version(m_executable));
}

} // namespace Internal
} // namespace MesonProjectManager

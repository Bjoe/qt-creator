// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "cmakeprojectplugin.h"

#include "cmakebuildconfiguration.h"
#include "cmakebuildstep.h"
#include "cmakebuildsystem.h"
#include "cmakeeditor.h"
#include "cmakekitinformation.h"
#include "cmakelocatorfilter.h"
#include "cmakeproject.h"
#include "cmakeprojectconstants.h"
#include "cmakeprojectmanager.h"
#include "cmakeprojectmanagertr.h"
#include "cmakeprojectnodes.h"
#include "cmakesettingspage.h"
#include "cmakespecificsettings.h"
#include "cmaketoolmanager.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/icore.h>

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/projecttree.h>

#include <texteditor/snippets/snippetprovider.h>

#include <utils/fsengine/fileiconprovider.h>
#include <utils/parameteraction.h>

#include <QTimer>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager::Internal {

class CMakeProjectPluginPrivate
{
public:
    CMakeToolManager cmakeToolManager; // have that before the first CMakeKitAspect

    ParameterAction buildTargetContextAction{
        Tr::tr("Build"),
        Tr::tr("Build \"%1\""),
        ParameterAction::AlwaysEnabled/*handled manually*/
    };

    CMakeSettingsPage settingsPage;
    CMakeSpecificSettingsPage specificSettings{CMakeProjectPlugin::projectTypeSpecificSettings()};

    CMakeManager manager;
    CMakeBuildStepFactory buildStepFactory;
    CMakeBuildConfigurationFactory buildConfigFactory;
    CMakeEditorFactory editorFactor;
    BuildCMakeTargetLocatorFilter buildCMakeTargetLocatorFilter;
    OpenCMakeTargetLocatorFilter openCMakeTargetLocationFilter;

    CMakeKitAspect cmakeKitAspect;
    CMakeGeneratorKitAspect cmakeGeneratorKitAspect;
    CMakeConfigurationKitAspect cmakeConfigurationKitAspect;
};

CMakeSpecificSettings *CMakeProjectPlugin::projectTypeSpecificSettings()
{
    static CMakeSpecificSettings theSettings;
    return &theSettings;
}

CMakeProjectPlugin::~CMakeProjectPlugin()
{
    delete d;
}

bool CMakeProjectPlugin::initialize(const QStringList & /*arguments*/, QString *errorMessage)
{
    Q_UNUSED(errorMessage)

    d = new CMakeProjectPluginPrivate;
    projectTypeSpecificSettings()->readSettings(ICore::settings());

    const Context projectContext{CMakeProjectManager::Constants::CMAKE_PROJECT_ID};

    FileIconProvider::registerIconOverlayForSuffix(Constants::Icons::FILE_OVERLAY, "cmake");
    FileIconProvider::registerIconOverlayForFilename(Constants::Icons::FILE_OVERLAY,
                                                     "CMakeLists.txt");

    TextEditor::SnippetProvider::registerGroup(Constants::CMAKE_SNIPPETS_GROUP_ID,
                                               Tr::tr("CMake", "SnippetProvider"));
    ProjectManager::registerProjectType<CMakeProject>(Constants::CMAKE_PROJECT_MIMETYPE);

    //register actions
    Command *command = ActionManager::registerAction(&d->buildTargetContextAction,
                                                     Constants::BUILD_TARGET_CONTEXT_MENU,
                                                     projectContext);
    command->setAttribute(Command::CA_Hide);
    command->setAttribute(Command::CA_UpdateText);
    command->setDescription(d->buildTargetContextAction.text());

    ActionManager::actionContainer(ProjectExplorer::Constants::M_SUBPROJECTCONTEXT)
            ->addAction(command, ProjectExplorer::Constants::G_PROJECT_BUILD);

    // Wire up context menu updates:
    connect(ProjectTree::instance(), &ProjectTree::currentNodeChanged,
            this, &CMakeProjectPlugin::updateContextActions);

    connect(&d->buildTargetContextAction, &ParameterAction::triggered, this, [] {
        if (auto bs = qobject_cast<CMakeBuildSystem *>(ProjectTree::currentBuildSystem())) {
            auto targetNode = dynamic_cast<const CMakeTargetNode *>(ProjectTree::currentNode());
            bs->buildCMakeTarget(targetNode ? targetNode->displayName() : QString());
        }
    });

    return true;
}

void CMakeProjectPlugin::extensionsInitialized()
{
    // Delay the restoration to allow the devices to load first.
    QTimer::singleShot(0, this, [] { CMakeToolManager::restoreCMakeTools(); });
}

void CMakeProjectPlugin::updateContextActions(Node *node)
{
    auto targetNode = dynamic_cast<const CMakeTargetNode *>(node);
    const QString targetDisplayName = targetNode ? targetNode->displayName() : QString();

    // Build Target:
    d->buildTargetContextAction.setParameter(targetDisplayName);
    d->buildTargetContextAction.setEnabled(targetNode);
    d->buildTargetContextAction.setVisible(targetNode);
}

} // CMakeProjectManager::Internal


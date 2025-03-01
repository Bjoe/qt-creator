// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "qnxanalyzesupport.h"

#include "qnxtr.h"
#include "slog2inforunner.h"

#include <projectexplorer/devicesupport/deviceusedportsgatherer.h>

#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>

#include <qmldebug/qmldebugcommandlinearguments.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace Qnx::Internal {

QnxQmlProfilerSupport::QnxQmlProfilerSupport(RunControl *runControl)
    : SimpleTargetRunner(runControl)
{
    setId("QnxQmlProfilerSupport");
    appendMessage(Tr::tr("Preparing remote side..."), Utils::LogMessageFormat);

    auto portsGatherer = new PortsGatherer(runControl);
    addStartDependency(portsGatherer);

    auto slog2InfoRunner = new Slog2InfoRunner(runControl);
    addStartDependency(slog2InfoRunner);

    auto profiler = runControl->createWorker(ProjectExplorer::Constants::QML_PROFILER_RUNNER);
    profiler->addStartDependency(this);
    addStopDependency(profiler);

    setStartModifier([this, portsGatherer, profiler] {
        const QUrl serverUrl = portsGatherer->findEndPoint();
        profiler->recordData("QmlServerUrl", serverUrl);

        CommandLine cmd = commandLine();
        cmd.addArg(QmlDebug::qmlDebugTcpArguments(QmlDebug::QmlProfilerServices, serverUrl));
        setCommandLine(cmd);
    });
}

} // Qnx::Internal

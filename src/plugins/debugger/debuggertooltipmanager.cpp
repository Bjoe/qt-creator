// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "debuggertooltipmanager.h"

#include "debuggeractions.h"
#include "debuggerengine.h"
#include "debuggerinternalconstants.h"
#include "debuggermainwindow.h"
#include "debuggerprotocol.h"
#include "debuggertr.h"
#include "sourceutils.h"
#include "stackhandler.h"
#include "watchhandler.h"

#include <coreplugin/icore.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/editormanager/documentmodel.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/modemanager.h>

#include <cppeditor/cppprojectfile.h>

#include <projectexplorer/session.h>

#include <texteditor/texteditor.h>
#include <texteditor/textdocument.h>

#include <utils/algorithm.h>
#include <utils/qtcassert.h>
#include <utils/tooltip/tooltip.h>
#include <utils/stringutils.h>
#include <utils/treemodel.h>
#include <utils/utilsicons.h>

#include <QAbstractItemModel>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QFileInfo>
#include <QLabel>
#include <QScreen>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStack>
#include <QStandardItemModel>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

using namespace Core;
using namespace ProjectExplorer;
using namespace TextEditor;
using namespace Utils;

namespace Debugger::Internal {

//#define DEBUG(x) qDebug() << x
#define DEBUG(x)

// Expire tooltips after n days on (no longer load them) in order
// to avoid them piling up.
enum { toolTipsExpiryDays = 6 };

const char sessionSettingsKeyC[] = "DebuggerToolTips";
const char sessionDocumentC[] = "DebuggerToolTips";
const char sessionVersionAttributeC[] = "version";
const char toolTipElementC[] = "DebuggerToolTip";
//const char toolTipClassAttributeC[] = "class";
const char fileNameAttributeC[] = "name";
const char functionAttributeC[] = "function";
const char textPositionAttributeC[] = "position";
const char textLineAttributeC[] = "line";
const char textColumnAttributeC[] = "column";
const char offsetXAttributeC[] = "offset_x";
const char offsetYAttributeC[] = "offset_y";
const char engineTypeAttributeC[] = "engine";
const char dateAttributeC[] = "date";
const char treeElementC[] = "tree";
const char treeExpressionAttributeC[] = "expression";
const char treeInameAttributeC[] = "iname";
// const char modelElementC[] = "model";
// const char modelColumnCountAttributeC[] = "columncount";
// const char modelRowElementC[] = "row";
const char modelItemElementC[] = "item";

class DebuggerToolTipHolder;

class DebuggerToolTipManagerPrivate : public QObject
{
public:
    explicit DebuggerToolTipManagerPrivate(DebuggerEngine *engine);

    void slotTooltipOverrideRequested(TextEditor::TextEditorWidget *editorWidget,
                                      const QPoint &point, int pos, bool *handled);
    void slotEditorOpened(Core::IEditor *e);
    void hideAllToolTips();
    void purgeClosedToolTips();

    void onModeChanged(Id mode)
    {
        if (mode == Constants::MODE_DEBUG) {
            //        if (EngineManager::engines().isEmpty())
            //            DebuggerMainWindow::instance()->restorePerspective(Constants::PRESET_PERSPRECTIVE_ID);
            debugModeEntered();
        } else {
            leavingDebugMode();
        }
    }

    void setupEditors();

    void debugModeEntered();
    void leavingDebugMode();

    void sessionAboutToChange();
    void loadSessionData();
    void saveSessionData();

    void updateVisibleToolTips();
    void closeAllToolTips();

    bool eventFilter(QObject *, QEvent *) override;

public:
    DebuggerEngine *m_engine;
    QVector<DebuggerToolTipHolder *> m_tooltips;
    bool m_debugModeActive = false;
};

// Forward a stream reader across end elements looking for the
// next start element of a desired type.
static bool readStartElement(QXmlStreamReader &r, const char *name)
{
    while (r.tokenType() != QXmlStreamReader::StartElement
            || r.name() != QLatin1String(name))
        switch (r.readNext()) {
        case QXmlStreamReader::EndDocument:
            return false;
        case QXmlStreamReader::NoToken:
        case QXmlStreamReader::Invalid:
            qWarning("'%s'/'%s' encountered while looking for start element '%s'.",
                    qPrintable(r.tokenString()),
                    qPrintable(r.name().toString()), name);
            return false;
        default:
            break;
        }
    return true;
}

// A label that can be dragged to drag something else.

class DraggableLabel : public QLabel
{
public:
    explicit DraggableLabel(QWidget *target)
        : m_target(target), m_moveStartPos(-1, -1), active(false)
    {}

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

public:
    QWidget *m_target;
    QPoint m_moveStartPos;
    QPoint m_offset;
    bool active;
};

void DraggableLabel::mousePressEvent(QMouseEvent * event)
{
    if (active && event->button() == Qt::LeftButton) {
        m_moveStartPos = event->globalPos();
        event->accept();
    }
    QLabel::mousePressEvent(event);
}

void DraggableLabel::mouseReleaseEvent(QMouseEvent * event)
{
    if (active && event->button() == Qt::LeftButton)
        m_moveStartPos = QPoint(-1, -1);
    QLabel::mouseReleaseEvent(event);
}

void DraggableLabel::mouseMoveEvent(QMouseEvent * event)
{
    if (active && (event->buttons() & Qt::LeftButton)) {
        if (m_moveStartPos != QPoint(-1, -1)) {
            const QPoint newPos = event->globalPos();
            const QPoint offset = newPos - m_moveStartPos;

            m_target->move(m_target->pos() + offset);
            m_offset += offset;

            m_moveStartPos = newPos;
        }
        event->accept();
    }
    QLabel::mouseMoveEvent(event);
}

/////////////////////////////////////////////////////////////////////////
//
// ToolTipWatchItem
//
/////////////////////////////////////////////////////////////////////////

class ToolTipWatchItem : public TreeItem
{
public:
    ToolTipWatchItem() = default;
    ToolTipWatchItem(TreeItem *item);

    bool hasChildren() const override { return expandable; }
    bool canFetchMore() const override { return childCount() == 0 && expandable && model(); }
    void fetchMore() override {}
    QVariant data(int column, int role) const override;

public:
    QString name;
    QString value;
    QString type;
    QString expression;
    QColor valueColor;
    bool expandable = false;
    QString iname;
};

ToolTipWatchItem::ToolTipWatchItem(TreeItem *item)
{
    const QAbstractItemModel *model = item->model();
    QModelIndex idx = item->index();
    name = model->data(idx.sibling(idx.row(), WatchModelBase::NameColumn), Qt::DisplayRole).toString();
    value = model->data(idx.sibling(idx.row(), WatchModelBase::ValueColumn), Qt::DisplayRole).toString();
    type = model->data(idx.sibling(idx.row(), WatchModelBase::TypeColumn), Qt::DisplayRole).toString();
    iname = model->data(idx.sibling(idx.row(), WatchModelBase::NameColumn), LocalsINameRole).toString();
    valueColor = model->data(idx.sibling(idx.row(), WatchModelBase::ValueColumn), Qt::ForegroundRole).value<QColor>();
    expandable = model->hasChildren(idx);
    expression = model->data(idx.sibling(idx.row(), WatchModelBase::NameColumn), Qt::EditRole).toString();
    for (TreeItem *child : *item)
        appendChild(new ToolTipWatchItem(child));
}

/////////////////////////////////////////////////////////////////////////
//
// ToolTipModel
//
/////////////////////////////////////////////////////////////////////////

class ToolTipModel : public TreeModel<ToolTipWatchItem>
{
public:
    ToolTipModel()
    {
        setHeader({Tr::tr("Name"), Tr::tr("Value"), Tr::tr("Type")});
        m_enabled = true;
        auto item = new ToolTipWatchItem;
        item->expandable = true;
        setRootItem(item);
    }

    void expandNode(const QModelIndex &idx)
    {
        m_expandedINames.insert(idx.data(LocalsINameRole).toString());
        if (canFetchMore(idx))
            fetchMore(idx);
    }

    void collapseNode(const QModelIndex &idx)
    {
        m_expandedINames.remove(idx.data(LocalsINameRole).toString());
    }

    void fetchMore(const QModelIndex &idx) override
    {
        if (!idx.isValid())
            return;
        auto item = dynamic_cast<ToolTipWatchItem *>(itemForIndex(idx));
        if (!item)
            return;
        QString iname = item->iname;
        if (!m_engine)
            return;

        WatchItem *it = m_engine->watchHandler()->findItem(iname);
        QTC_ASSERT(it, return);
        it->model()->fetchMore(it->index());
    }

    void restoreTreeModel(QXmlStreamReader &r);

    QPointer<DebuggerEngine> m_engine;
    QSet<QString> m_expandedINames;
    bool m_enabled;
};

QVariant ToolTipWatchItem::data(int column, int role) const
{
    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case 0:
                    return name;
                case 1:
                    return value;
                case 2:
                    return type;
            }
            break;
        }

        case LocalsINameRole:
            return iname;

        case Qt::ForegroundRole:
            if (model() && static_cast<ToolTipModel *>(model())->m_enabled) {
                if (column == 1)
                    return valueColor;
                return QVariant();
            }
            return QColor(140, 140, 140);

        default:
            break;
    }
    return QVariant();
}

void ToolTipModel::restoreTreeModel(QXmlStreamReader &r)
{
    Q_UNUSED(r)
#if 0
// Helper for building a QStandardItemModel of a tree form (see TreeModelVisitor).
// The recursion/building is based on the scheme: \code
// <row><item1><item2>
//     <row><item11><item12></row>
// </row>
// \endcode

    bool withinModel = true;
    while (withinModel && !r.atEnd()) {
        const QXmlStreamReader::TokenType token = r.readNext();
        switch (token) {
        case QXmlStreamReader::StartElement: {
            const QStringRef element = r.name();
            // Root model element with column count.
            if (element == QLatin1String(modelElementC)) {
                if (const int cc = r.attributes().value(QLatin1String(modelColumnCountAttributeC)).toString().toInt())
                    columnCount = cc;
                m->setColumnCount(columnCount);
            } else if (element == QLatin1String(modelRowElementC)) {
                builder.startRow();
            } else if (element == QLatin1String(modelItemElementC)) {
                builder.addItem(r.readElementText());
            }
        }
            break; // StartElement
        case QXmlStreamReader::EndElement: {
            const QStringRef element = r.name();
            // Row closing: pop off parent.
            if (element == QLatin1String(modelRowElementC))
                builder.endRow();
            else if (element == QLatin1String(modelElementC))
                withinModel = false;
        }
            break; // EndElement
        default:
            break;
        } // switch
    } // while
#endif
}

/*!
    \class Debugger::Internal::DebuggerToolTipTreeView

    \brief The DebuggerToolTipTreeView class is a treeview that adapts its size
    to the model contents (also while expanding)
    to be used within DebuggerTreeViewToolTipWidget.

*/

class DebuggerToolTipTreeView : public QTreeView
{
public:
    explicit DebuggerToolTipTreeView(QWidget *parent)
        : QTreeView(parent)
    {
        setHeaderHidden(true);
        setEditTriggers(NoEditTriggers);
        setUniformRowHeights(true);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    QSize sizeHint() const override { return m_size; }

    int sizeHintForColumn(int column) const override
    {
        return QTreeView::sizeHintForColumn(column);
    }

    int computeHeight(const QModelIndex &index) const
    {
        int s = rowHeight(index);
        const int rowCount = model()->rowCount(index);
        for (int i = 0; i < rowCount; ++i)
            s += computeHeight(model()->index(i, 0, index));
        return s;
    }

    QSize m_size;
};


/////////////////////////////////////////////////////////////////////////
//
// DebuggerToolTipWidget
//
/////////////////////////////////////////////////////////////////////////

class DebuggerToolTipWidget : public QWidget
{
public:
    DebuggerToolTipWidget();

    ~DebuggerToolTipWidget() override { DEBUG("DESTROY DEBUGGERTOOLTIP WIDGET"); }

    void closeEvent(QCloseEvent *) override { DEBUG("CLOSE DEBUGGERTOOLTIP WIDGET"); }

    void enterEvent(QEnterEvent *) override { DEBUG("ENTER DEBUGGERTOOLTIP WIDGET"); }

    void leaveEvent(QEvent *) override
    {
        DEBUG("LEAVE DEBUGGERTOOLTIP WIDGET");
        if (BaseTextEditor *editor = BaseTextEditor::currentTextEditor())
            editor->editorWidget()->activateWindow();
    }

    void pin()
    {
        if (isPinned)
            return;
        isPinned = true;
        pinButton->setIcon(style()->standardIcon(QStyle::SP_DockWidgetCloseButton));

        if (parentWidget()) {
            // We are currently within a text editor tooltip:
            // Rip out of parent widget and re-show as a tooltip
            // Find parent with different window than the tooltip itself:
            QWidget *top = parentWidget();
            while (top->window() == window() && top->parentWidget())
                top = top->parentWidget();
            ToolTip::pinToolTip(this, top->window());
        } else {
            // We have just be restored from session data.
            setWindowFlags(Qt::ToolTip);
        }
        titleLabel->active = true; // User can now drag
    }

    void computeSize();

    void setContents(ToolTipWatchItem *item)
    {
        titleLabel->setText(item->expression);
        //treeView->setEnabled(true);
        model.m_enabled = true;
        if (item) {
            model.rootItem()->removeChildren();
            model.rootItem()->appendChild(item);
        }
        reexpand(QModelIndex());
        computeSize();
    }

    WatchHandler *watchHandler() const
    {
        return model.m_engine->watchHandler();
    }

    void setEngine(DebuggerEngine *engine) { model.m_engine = engine; }

    void reexpand(const QModelIndex &idx)
    {
        TreeItem *item = model.itemForIndex(idx);
        QTC_ASSERT(item, return);
        QString iname = item->data(0, LocalsINameRole).toString();
        bool shouldExpand = model.m_expandedINames.contains(iname);
        if (shouldExpand) {
            if (!treeView->isExpanded(idx)) {
                treeView->expand(idx);
                for (int i = 0, n = model.rowCount(idx); i != n; ++i) {
                    QModelIndex idx1 = model.index(i, 0, idx);
                    reexpand(idx1);
                }
            }
        } else {
            if (treeView->isExpanded(idx))
                treeView->collapse(idx);
        }
    }

public:
    bool isPinned;
    QToolButton *pinButton;
    DraggableLabel *titleLabel;
    DebuggerToolTipTreeView *treeView;
    ToolTipModel model;
};

DebuggerToolTipWidget::DebuggerToolTipWidget()
{
    setAttribute(Qt::WA_DeleteOnClose);

    isPinned = false;
    const QIcon pinIcon(":/debugger/images/pin.xpm");

    pinButton = new QToolButton;
    pinButton->setIcon(pinIcon);

    auto copyButton = new QToolButton;
    copyButton->setToolTip(Tr::tr("Copy Contents to Clipboard"));
    copyButton->setIcon(Utils::Icons::COPY.icon());

    titleLabel = new DraggableLabel(this);
    titleLabel->setMinimumWidth(40); // Ensure a draggable area even if text is empty.
    titleLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

    auto toolBar = new QToolBar(this);
    toolBar->setProperty("_q_custom_style_disabled", QVariant(true));
    const QList<QSize> pinIconSizes = pinIcon.availableSizes();
    if (!pinIconSizes.isEmpty())
        toolBar->setIconSize(pinIconSizes.front());
    toolBar->addWidget(pinButton);
    toolBar->addWidget(copyButton);
    toolBar->addWidget(titleLabel);

    treeView = new DebuggerToolTipTreeView(this);
    treeView->setFocusPolicy(Qt::NoFocus);
    treeView->setModel(&model);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(toolBar);
    mainLayout->addWidget(treeView);

    connect(copyButton, &QAbstractButton::clicked, this, [this] {
        QString text;
        QTextStream str(&text);
        model.forAllItems([&str](ToolTipWatchItem *item) {
            str << QString(item->level(), '\t')
                << item->name << '\t' << item->value << '\t' << item->type << '\n';
        });
        setClipboardAndSelection(text);
    });

    connect(treeView, &QTreeView::expanded, &model, &ToolTipModel::expandNode);
    connect(treeView, &QTreeView::collapsed, &model, &ToolTipModel::collapseNode);

    connect(treeView, &QTreeView::collapsed, this, &DebuggerToolTipWidget::computeSize,
        Qt::QueuedConnection);
    connect(treeView, &QTreeView::expanded, this, &DebuggerToolTipWidget::computeSize,
        Qt::QueuedConnection);
    DEBUG("CREATE DEBUGGERTOOLTIP WIDGET");
}

void DebuggerToolTipWidget::computeSize()
{
    int columns = 30; // Decoration
    int rows = 0;
    bool rootDecorated = false;

    reexpand(model.index(0, 0, QModelIndex()));
    const int columnCount = model.columnCount(QModelIndex());
    rootDecorated = model.rowCount() > 0;
    if (rootDecorated) {
        for (int i = 0; i < columnCount; ++i) {
            treeView->resizeColumnToContents(i);
            columns += treeView->sizeHintForColumn(i);
        }
    }
    if (columns < 100)
        columns = 100; // Prevent toolbar from shrinking when displaying 'Previous'
    rows += treeView->computeHeight(QModelIndex());

    // Fit tooltip to screen, showing/hiding scrollbars as needed.
    // Add a bit of space to account for tooltip border, and not
    // touch the border of the screen.
    QPoint pos(x(), y());
    auto screen = QGuiApplication::screenAt(pos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect desktopRect = screen->availableGeometry();
    const int maxWidth = desktopRect.right() - pos.x() - 5 - 5;
    const int maxHeight = desktopRect.bottom() - pos.y() - 5 - 5;

    if (columns > maxWidth)
        rows += treeView->horizontalScrollBar()->height();

    if (rows > maxHeight) {
        treeView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        rows = maxHeight;
        columns += treeView->verticalScrollBar()->width();
    } else {
        treeView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    if (columns > maxWidth) {
        treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        columns = maxWidth;
    } else {
        treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    treeView->m_size = QSize(columns + 5, rows + 5);
    treeView->setMinimumSize(treeView->m_size);
    treeView->setMaximumSize(treeView->m_size);
    treeView->setRootIsDecorated(rootDecorated);
}


/////////////////////////////////////////////////////////////////////////
//
// DebuggerToolTipHolder
//
/////////////////////////////////////////////////////////////////////////

enum DebuggerTooltipState
{
    New, // All new, widget not shown, not async (yet)
    PendingUnshown, // Widget not (yet) shown, async.
    PendingShown, // Widget shown, async
    Acquired, // Widget shown sync, engine attached
    Released // Widget shown, engine released
};

class DebuggerToolTipHolder
{
public:
    DebuggerToolTipHolder(const DebuggerToolTipContext &context);
    ~DebuggerToolTipHolder() { delete widget; }

    void acquireEngine(DebuggerEngine *engine);
    void releaseEngine();

    void saveSessionData(QXmlStreamWriter &w) const;

    void positionShow(const TextEditorWidget *editorWidget);

    void updateTooltip(DebuggerEngine *engine);

    void setState(DebuggerTooltipState newState);
    void destroy();

public:
    QPointer<DebuggerToolTipWidget> widget;
    QDate creationDate;
    DebuggerToolTipContext context;
    DebuggerTooltipState state;
};

void DebuggerToolTipManagerPrivate::hideAllToolTips()
{
    purgeClosedToolTips();
    for (const DebuggerToolTipHolder *tooltip : std::as_const(m_tooltips))
        tooltip->widget->hide();
}

/*!
    \class Debugger::Internal::DebuggerToolTipContext

    \brief The DebuggerToolTipContext class specifies the file name and
    position where the tooltip is anchored.

    Uses redundant position or line column information to detect whether
    the underlying file has been changed
    on restoring.
*/

DebuggerToolTipContext::DebuggerToolTipContext()
    : position(0), line(0), column(0), scopeFromLine(0), scopeToLine(0), isCppEditor(true)
{
}

static bool filesMatch(const FilePath &file1, const FilePath &file2)
{
    return file1.canonicalPath() == file2.canonicalPath();
}

bool DebuggerToolTipContext::matchesFrame(const StackFrame &frame) const
{
    return (fileName.isEmpty() || frame.file.isEmpty() || filesMatch(fileName, frame.file))
            //&& (function.isEmpty() || frame.function.isEmpty() || function == frame.function);
            && (frame.line <= 0 || (scopeFromLine <= frame.line && frame.line <= scopeToLine));
}

bool DebuggerToolTipContext::isSame(const DebuggerToolTipContext &other) const
{
    return iname == other.iname
        && scopeFromLine == other.scopeFromLine
        && scopeToLine == other.scopeToLine
        && filesMatch(fileName, other.fileName);
}

QString DebuggerToolTipContext::toolTip() const
{
    return Tr::tr("Expression %1 in function %2 from line %3 to %4")
            .arg(expression).arg(function).arg(scopeFromLine).arg(scopeToLine);
}

QDebug operator<<(QDebug d, const DebuggerToolTipContext &c)
{
    QDebug nsp = d.nospace();
    nsp << c.fileName << '@' << c.line << ',' << c.column << " (" << c.position << ')'
        << "INAME: " << c.iname << " EXP: " << c.expression << " FUNCTION: " << c.function;
    return d;
}

/*!
    \class Debugger::Internal::DebuggerToolTipWidget

    \brief The DebuggerToolTipWidget class is a pinnable debugger tool tip
    widget.

    The debugger tooltip goes from the unpinned state (button
    showing 'Pin') to the pinned state (button showing 'Close').
    It consists of a title toolbar and a vertical main layout.
    The widget has the ability to save/restore tree model contents to XML.
    With the engine acquired, it sets a filter model (by expression) on
    one of the engine's models (debuggerModel).
    On release, it serializes and restores the data to a QStandardItemModel
    (defaultModel) and displays that.

    It is associated with file name and position with functionality to
    acquire and release the engine. When the debugger stops at a file, all
    matching tooltips acquire the engine, that is, display the engine data.
    When continuing or switching away from the frame, the tooltips release the
    engine, that is, store the data internally and keep displaying them
    marked as 'previous'.

    When restoring the data from a session, all tooltips start in 'released' mode.

    Stored tooltips expire after toolTipsExpiryDays while loading to prevent
    them accumulating.

    In addition, if the stored line number diverges too much from the current line
    number in positionShow(), the tooltip is also closed/discarded.

    The widget is that is first shown by the TextEditor's tooltip
    class and typically closed by it unless the user pins it.
    In that case, it is removed from the tip's layout, added to the DebuggerToolTipManager's
    list of pinned tooltips and re-shown as a global tooltip widget.
    As the debugger stop and continues, it shows the debugger values or a copy
    of them. On closing or session changes, the contents it saved.
*/

DebuggerToolTipHolder::DebuggerToolTipHolder(const DebuggerToolTipContext &context_)
{
    widget = new DebuggerToolTipWidget;
    widget->setObjectName("DebuggerTreeViewToolTipWidget: " + context_.iname);

    context = context_;
    context.creationDate = QDate::currentDate();

    state = New;

    QObject::connect(widget->pinButton, &QAbstractButton::clicked, [this] {
        if (widget->isPinned)
            widget->close();
        else
            widget->pin();
    });
}



// This is called back from the engines after they populated the
// WatchModel. If the populating result from evaluation of this
// tooltip here, we are in "PendingUnshown" state (no Widget show yet),
// or "PendingShown" state (old widget reused).
//
// If we are in "Acquired" or "Released", this is an update
// after normal WatchModel update.

void DebuggerToolTipHolder::updateTooltip(DebuggerEngine *engine)
{
    widget->setEngine(engine);

    if (!engine) {
        setState(Released);
        return;
    }

    StackFrame frame = engine->stackHandler()->currentFrame();
    WatchItem *item = engine->watchHandler()->findItem(context.iname);

    // FIXME: The engine should decide on whether it likes
    // the context.
    const bool sameFrame = context.matchesFrame(frame)
        || context.fileName.endsWith(".py");
    DEBUG("UPDATE TOOLTIP: STATE " << state << context.iname
          << "PINNED: " << widget->isPinned
          << "SHOW NEEDED: " << widget->isPinned
          << "SAME FRAME: " << sameFrame);

    if (state == PendingUnshown) {
        setState(PendingShown);
        ToolTip::show(context.mousePosition, widget, DebuggerMainWindow::instance());
    }

    if (item && sameFrame) {
        DEBUG("ACQUIRE ENGINE: STATE " << state);
        widget->setContents(new ToolTipWatchItem(item));
    } else {
        releaseEngine();
    }
    widget->titleLabel->setToolTip(context.toolTip());
}

void DebuggerToolTipHolder::setState(DebuggerTooltipState newState)
{
    bool ok = (state == New && newState == PendingUnshown)
        || (state == New && newState == Acquired)
        || (state == PendingUnshown && newState == PendingShown)
        || newState == Released;

    DEBUG("TRANSITION STATE FROM " << state << " TO " << newState);
    QTC_ASSERT(ok, qDebug() << "Unexpected tooltip state transition from "
                            << state << " to " << newState);

    state = newState;
}

void DebuggerToolTipHolder::destroy()
{
    if (widget) {
        widget->close();
        widget = nullptr;
    }
}

void DebuggerToolTipHolder::releaseEngine()
{
    DEBUG("RELEASE ENGINE: STATE " << state);
    if (state == Released)
        return;

    QTC_ASSERT(widget, return);
    if (state == PendingShown) {
        setState(Released);
        // This happens after hovering over something that looks roughly like
        // a valid expression but can't be resolved by the debugger backend.
        // (Out of scope items, keywords, ...)
        ToolTip::show(context.mousePosition,
                      Tr::tr("No valid expression"),
                      DebuggerMainWindow::instance());
        widget->deleteLater();
        return;
    }

    setState(Released);
    widget->model.m_enabled = false;
    emit widget->model.layoutChanged();
    widget->titleLabel->setText(Tr::tr("%1 (Previous)").arg(context.expression));
}

void DebuggerToolTipHolder::positionShow(const TextEditorWidget *editorWidget)
{
    // Figure out new position of tooltip using the text edit.
    // If the line changed too much, close this tip.
    QTC_ASSERT(editorWidget, return);
    QTextCursor cursor = editorWidget->textCursor();
    cursor.setPosition(context.position);
    const int line = cursor.blockNumber();
    if (qAbs(context.line - line) > 2) {
        widget->close();
        return ;
    }

    const QPoint screenPos = editorWidget->toolTipPosition(cursor) + widget->titleLabel->m_offset;
    const QRect toolTipArea = QRect(screenPos, QSize(widget->sizeHint()));
    const QRect plainTextArea = QRect(editorWidget->mapToGlobal(QPoint(0, 0)), editorWidget->size());
    const bool visible = plainTextArea.intersects(toolTipArea);
    //    DEBUG("DebuggerToolTipWidget::positionShow() " << this << m_context
    //             << " line: " << line << " plainTextPos " << toolTipArea
    //             << " offset: " << m_titleLabel->m_offset
    //             << " Area: " << plainTextArea << " Screen pos: "
    //             << screenPos << te.widget << " visible=" << visible);

    if (visible) {
        widget->move(screenPos);
        widget->show();
    } else {
        widget->hide();
    }
}

//// Parse a 'yyyyMMdd' date
static QDate dateFromString(const QString &date)
{
    return date.size() == 8
               ? QDate(date.left(4).toInt(), date.mid(4, 2).toInt(), date.mid(6, 2).toInt())
               : QDate();
}

void DebuggerToolTipHolder::saveSessionData(QXmlStreamWriter &w) const
{
    w.writeStartElement(toolTipElementC);
    QXmlStreamAttributes attributes;
//    attributes.append(toolTipClassAttributeC, QString::fromLatin1(metaObject()->className()));
    attributes.append(fileNameAttributeC, context.fileName.toString());
    if (!context.function.isEmpty())
        attributes.append(functionAttributeC, context.function);
    attributes.append(textPositionAttributeC, QString::number(context.position));
    attributes.append(textLineAttributeC, QString::number(context.line));
    attributes.append(textColumnAttributeC, QString::number(context.column));
    attributes.append(dateAttributeC, creationDate.toString("yyyyMMdd"));
    QPoint offset = widget->titleLabel->m_offset;
    if (offset.x())
        attributes.append(offsetXAttributeC, QString::number(offset.x()));
    if (offset.y())
        attributes.append(offsetYAttributeC, QString::number(offset.y()));
    attributes.append(engineTypeAttributeC, context.engineType);
    attributes.append(treeExpressionAttributeC, context.expression);
    attributes.append(treeInameAttributeC, context.iname);
    w.writeAttributes(attributes);

    w.writeStartElement(treeElementC);
    widget->model.forAllItems([&w](ToolTipWatchItem *item) {
        const QString modelItemElement(modelItemElementC);
        for (int i = 0; i < 3; ++i) {
            const QString value = item->data(i, Qt::DisplayRole).toString();
            if (value.isEmpty())
                w.writeEmptyElement(modelItemElement);
            else
                w.writeTextElement(modelItemElement, value);
        }
    });
    w.writeEndElement();

    w.writeEndElement();
}

/*!
    \class Debugger::Internal::DebuggerToolTipManager

    \brief The DebuggerToolTipManager class manages the pinned tooltip widgets,
    listens on editor scroll and main window move
    events and takes care of repositioning the tooltips.

    Listens to editor change and mode change. In debug mode, if there are tooltips
    for the current editor (by file name), positions and shows them.

    In addition, listens on state change and stack frame completed signals
    of the engine. If a stack frame is completed, has all matching tooltips
    (by file name and function) acquire the engine, others release.
*/

DebuggerToolTipManager::DebuggerToolTipManager(DebuggerEngine *engine)
    : d(new DebuggerToolTipManagerPrivate(engine))
{
}

DebuggerToolTipManager::~DebuggerToolTipManager()
{
    delete d;
}

void DebuggerToolTipManagerPrivate::updateVisibleToolTips()
{
    purgeClosedToolTips();
    if (m_tooltips.isEmpty())
        return;
    if (!m_debugModeActive) {
        hideAllToolTips();
        return;
    }

    BaseTextEditor *toolTipEditor = BaseTextEditor::currentTextEditor();
    if (!toolTipEditor) {
        hideAllToolTips();
        return;
    }

    const FilePath filePath = toolTipEditor->textDocument()->filePath();
    if (filePath.isEmpty()) {
        hideAllToolTips();
        return;
    }

    // Reposition and show all tooltips of that file.
    for (DebuggerToolTipHolder *tooltip : std::as_const(m_tooltips)) {
        if (tooltip->context.fileName == filePath)
            tooltip->positionShow(toolTipEditor->editorWidget());
        else
            tooltip->widget->hide();
    }
}

void DebuggerToolTipManager::updateToolTips()
{
    d->purgeClosedToolTips();
    if (d->m_tooltips.isEmpty())
        return;

    // Stack frame changed: All tooltips of that file acquire the engine,
    // all others release (arguable, this could be more precise?)
    for (DebuggerToolTipHolder *tooltip : std::as_const(d->m_tooltips))
        tooltip->updateTooltip(d->m_engine);
    d->updateVisibleToolTips(); // Move tooltip when stepping in same file.
}

void DebuggerToolTipManager::deregisterEngine()
{
    DEBUG("DEREGISTER ENGINE");

    d->purgeClosedToolTips();

    for (DebuggerToolTipHolder *tooltip : std::as_const(d->m_tooltips))
        if (tooltip->context.engineType == d->m_engine->objectName())
            tooltip->releaseEngine();

    d->saveSessionData();

    // FIXME: For now remove all.
    for (DebuggerToolTipHolder *tooltip : std::as_const(d->m_tooltips))
        tooltip->destroy();
    d->purgeClosedToolTips();
}

bool DebuggerToolTipManager::hasToolTips() const
{
    return !d->m_tooltips.isEmpty();
}

void DebuggerToolTipManagerPrivate::sessionAboutToChange()
{
    closeAllToolTips();
}

void DebuggerToolTipManagerPrivate::loadSessionData()
{
    closeAllToolTips();
    const QString data = SessionManager::value(sessionSettingsKeyC).toString();
    QXmlStreamReader r(data);
    if (r.readNextStartElement() && r.name() == QLatin1String(sessionDocumentC)) {
        while (!r.atEnd()) {
            if (readStartElement(r, toolTipElementC)) {
                const QXmlStreamAttributes attributes = r.attributes();
                DebuggerToolTipContext context;
                context.fileName = FilePath::fromString(
                    attributes.value(fileNameAttributeC).toString());
                context.position = attributes.value(textPositionAttributeC).toString().toInt();
                context.line = attributes.value(textLineAttributeC).toString().toInt();
                context.column = attributes.value(textColumnAttributeC).toString().toInt();
                context.function = attributes.value(functionAttributeC).toString();
                QPoint offset;
                const QString offsetXAttribute(offsetXAttributeC);
                const QString offsetYAttribute(offsetYAttributeC);
                if (attributes.hasAttribute(offsetXAttribute))
                    offset.setX(attributes.value(offsetXAttribute).toString().toInt());
                if (attributes.hasAttribute(offsetYAttribute))
                    offset.setY(attributes.value(offsetYAttribute).toString().toInt());
                context.mousePosition = offset;

                context.iname = attributes.value(treeInameAttributeC).toString();
                context.expression = attributes.value(treeExpressionAttributeC).toString();

                //    const QStringRef className = attributes.value(toolTipClassAttributeC);
                context.engineType = attributes.value(engineTypeAttributeC).toString();
                context.creationDate = dateFromString(attributes.value(dateAttributeC).toString());
                bool readTree = context.isValid();
                if (!context.creationDate.isValid() || context.creationDate.daysTo(QDate::currentDate()) > toolTipsExpiryDays) {
                    // DEBUG("Expiring tooltip " << context.fileName << '@' << context.position << " from " << creationDate)
                    //readTree = false;
                } else { //if (className != "Debugger::Internal::DebuggerToolTipWidget") {
                    //qWarning("Unable to create debugger tool tip widget of class %s", qPrintable(className.toString()));
                    //readTree = false;
                }

                if (readTree) {
                    auto tw = new DebuggerToolTipHolder(context);
                    m_tooltips.push_back(tw);
                    tw->widget->model.restoreTreeModel(r);
                    tw->widget->pin();
                    tw->widget->titleLabel->setText(Tr::tr("%1 (Restored)").arg(context.expression));
                    tw->widget->treeView->expandAll();
                } else {
                    r.readElementText(QXmlStreamReader::SkipChildElements); // Skip
                }

                r.readNext(); // Skip </tree>
            }
        }
    }
}

void DebuggerToolTipManagerPrivate::saveSessionData()
{
    QString data;
    purgeClosedToolTips();

    QXmlStreamWriter w(&data);
    w.writeStartDocument();
    w.writeStartElement(sessionDocumentC);
    w.writeAttribute(sessionVersionAttributeC, "1.0");
    for (DebuggerToolTipHolder *tooltip : std::as_const(m_tooltips))
        if (tooltip->widget->isPinned)
            tooltip->saveSessionData(w);
    w.writeEndDocument();

    return; // FIXME
//    setSessionValue(sessionSettingsKeyC, QVariant(data));
}

void DebuggerToolTipManager::closeAllToolTips()
{
    d->closeAllToolTips();
}

void DebuggerToolTipManagerPrivate::closeAllToolTips()
{
    for (DebuggerToolTipHolder *tooltip : std::as_const(m_tooltips))
        tooltip->destroy();
    m_tooltips.clear();
}

void DebuggerToolTipManager::resetLocation()
{
    d->purgeClosedToolTips();
    for (DebuggerToolTipHolder *tooltip : std::as_const(d->m_tooltips))
        tooltip->widget->pin();
}

DebuggerToolTipManagerPrivate::DebuggerToolTipManagerPrivate(DebuggerEngine *engine)
    : m_engine(engine)
{
    connect(ModeManager::instance(), &ModeManager::currentModeChanged,
            this, &DebuggerToolTipManagerPrivate::onModeChanged);
    connect(SessionManager::instance(), &SessionManager::sessionLoaded,
            this, &DebuggerToolTipManagerPrivate::loadSessionData);
    connect(SessionManager::instance(), &SessionManager::aboutToSaveSession,
            this, &DebuggerToolTipManagerPrivate::saveSessionData);
    connect(SessionManager::instance(), &SessionManager::aboutToUnloadSession,
            this, &DebuggerToolTipManagerPrivate::sessionAboutToChange);
    debugModeEntered();
}

void DebuggerToolTipManagerPrivate::slotTooltipOverrideRequested
    (TextEditorWidget *editorWidget, const QPoint &point, int pos, bool *handled)
{
    QTC_ASSERT(handled, return);
    QTC_ASSERT(editorWidget, return);

    if (!debuggerSettings()->useToolTipsInMainEditor.value())
        return;

    const TextDocument *document = editorWidget->textDocument();
    if (!m_engine || !m_engine->canDisplayTooltip())
        return;

    DebuggerToolTipContext context;
    context.engineType = m_engine->objectName();
    context.fileName = document->filePath();
    context.position = pos;
    editorWidget->convertPosition(pos, &context.line, &context.column);
    QString raw = cppExpressionAt(editorWidget, context.position, &context.line, &context.column,
                                  &context.function, &context.scopeFromLine, &context.scopeToLine);
    context.expression = fixCppExpression(raw);
    context.isCppEditor = CppEditor::ProjectFile::classify(document->filePath().toString())
                            != CppEditor::ProjectFile::Unsupported;

    if (context.expression.isEmpty()) {
        ToolTip::show(point, Tr::tr("No valid expression"),
                             DebuggerMainWindow::instance());
        *handled = true;
        return;
    }

    purgeClosedToolTips();

    // Prefer a filter on an existing local variable if it can be found.
    const WatchItem *localVariable = m_engine->watchHandler()->findCppLocalVariable(context.expression);
    if (localVariable) {
        context.expression = localVariable->exp;
        if (context.expression.isEmpty())
            context.expression = localVariable->name;
        context.iname = localVariable->iname;

        auto reusable = [context] (DebuggerToolTipHolder *tooltip) {
            return tooltip->context.isSame(context);
        };
        DebuggerToolTipHolder *tooltip = Utils::findOrDefault(m_tooltips, reusable);
        if (tooltip) {
            DEBUG("REUSING LOCALS TOOLTIP");
            tooltip->context.mousePosition = point;
            ToolTip::move(point);
        } else {
            DEBUG("CREATING LOCALS, WAITING...");
            tooltip = new DebuggerToolTipHolder(context);
            tooltip->setState(Acquired);
            m_tooltips.push_back(tooltip);
            ToolTip::show(point, tooltip->widget, DebuggerMainWindow::instance());
        }
        DEBUG("SYNC IN STATE" << tooltip->state);
        tooltip->updateTooltip(m_engine);

    } else {

        context.iname = "tooltip." + toHex(context.expression);
        auto reusable = [context] (DebuggerToolTipHolder *tooltip) {
            return tooltip->context.isSame(context);
        };
        DebuggerToolTipHolder *tooltip = Utils::findOrDefault(m_tooltips, reusable);

        if (tooltip) {
            //tooltip->destroy();
            tooltip->context.mousePosition = point;
            ToolTip::move(point);
            DEBUG("UPDATING DELAYED.");
        } else {
            DEBUG("CREATING DELAYED.");
            tooltip = new DebuggerToolTipHolder(context);
            tooltip->context.mousePosition = point;
            m_tooltips.push_back(tooltip);
            tooltip->setState(PendingUnshown);
            if (m_engine->canHandleToolTip(context)) {
                m_engine->updateItem(context.iname);
            } else {
                ToolTip::show(point, Tr::tr("Expression too complex"),
                              DebuggerMainWindow::instance());
                tooltip->destroy();
            }
        }
    }

    *handled = true;
}

void DebuggerToolTipManagerPrivate::slotEditorOpened(IEditor *e)
{
    // Move tooltip along when scrolled.
    if (auto textEditor = qobject_cast<BaseTextEditor *>(e)) {
        TextEditorWidget *widget = textEditor->editorWidget();
        QObject::connect(widget->verticalScrollBar(), &QScrollBar::valueChanged,
                         this, &DebuggerToolTipManagerPrivate::updateVisibleToolTips);
        QObject::connect(widget, &TextEditorWidget::tooltipOverrideRequested,
                         this, &DebuggerToolTipManagerPrivate::slotTooltipOverrideRequested);
    }
}

void DebuggerToolTipManagerPrivate::debugModeEntered()
{
    // Hook up all signals in debug mode.
    if (!m_debugModeActive) {
        m_debugModeActive = true;
        QWidget *topLevel = ICore::mainWindow()->topLevelWidget();
        topLevel->installEventFilter(this);
        EditorManager *em = EditorManager::instance();
        connect(em, &EditorManager::currentEditorChanged,
                this, &DebuggerToolTipManagerPrivate::updateVisibleToolTips);
        connect(em, &EditorManager::editorOpened,
                this, &DebuggerToolTipManagerPrivate::slotEditorOpened);

        setupEditors();
    }
}

void DebuggerToolTipManagerPrivate::setupEditors()
{
    for (IEditor *e : DocumentModel::editorsForOpenedDocuments())
        slotEditorOpened(e);
    // Position tooltips delayed once all the editor placeholder layouting is done.
    if (!m_tooltips.isEmpty())
        QTimer::singleShot(0, this, &DebuggerToolTipManagerPrivate::updateVisibleToolTips);
}

void DebuggerToolTipManagerPrivate::leavingDebugMode()
{
    // Remove all signals in debug mode.
    if (m_debugModeActive) {
        m_debugModeActive = false;
        hideAllToolTips();
        if (QWidget *topLevel = ICore::mainWindow()->topLevelWidget())
            topLevel->removeEventFilter(this);
        const QList<IEditor *> editors = DocumentModel::editorsForOpenedDocuments();
        for (IEditor *e : editors) {
            if (auto toolTipEditor = qobject_cast<BaseTextEditor *>(e)) {
                toolTipEditor->editorWidget()->verticalScrollBar()->disconnect(this);
                toolTipEditor->editorWidget()->disconnect(this);
                toolTipEditor->disconnect(this);
            }
        }
        EditorManager::instance()->disconnect(this);
    }
}

DebuggerToolTipContexts DebuggerToolTipManager::pendingTooltips() const
{
    StackFrame frame = d->m_engine->stackHandler()->currentFrame();
    DebuggerToolTipContexts rc;
    for (DebuggerToolTipHolder *tooltip : std::as_const(d->m_tooltips)) {
        const DebuggerToolTipContext &context = tooltip->context;
        if (context.iname.startsWith("tooltip") && context.matchesFrame(frame))
            rc.push_back(context);
    }
    return rc;
}

bool DebuggerToolTipManagerPrivate::eventFilter(QObject *o, QEvent *e)
{
    if (m_tooltips.isEmpty())
        return false;
    switch (e->type()) {
    case QEvent::Move: { // Move along with parent (toplevel)
        const auto me = static_cast<const QMoveEvent *>(e);
        const QPoint dist = me->pos() - me->oldPos();
        purgeClosedToolTips();
        for (DebuggerToolTipHolder *tooltip : std::as_const(m_tooltips)) {
            if (tooltip->widget && tooltip->widget->isVisible())
                tooltip->widget->move(tooltip->widget->pos() + dist);
        }
        break;
    }
    case QEvent::WindowStateChange: { // Hide/Show along with parent (toplevel)
        const auto se = static_cast<const QWindowStateChangeEvent *>(e);
        const bool wasMinimized = se->oldState() & Qt::WindowMinimized;
        const bool isMinimized  = static_cast<const QWidget *>(o)->windowState() & Qt::WindowMinimized;
        if (wasMinimized != isMinimized) {
            purgeClosedToolTips();
            for (DebuggerToolTipHolder *tooltip : std::as_const(m_tooltips))
                tooltip->widget->setVisible(!isMinimized);
        }
        break;
    }
    default:
        break;
    }
    return false;
}

void DebuggerToolTipManagerPrivate::purgeClosedToolTips()
{
    for (int i = m_tooltips.size(); --i >= 0; ) {
        DebuggerToolTipHolder *tooltip = m_tooltips.at(i);
        if (!tooltip->widget) {
            DEBUG("PURGE TOOLTIP, LEFT: "  << m_tooltips.size());
            m_tooltips.removeAt(i);
        }
    }
}

} // Debugger::Internal

#ifndef CONTROLFLOWGRAPHVIEW_H
#define CONTROLFLOWGRAPHVIEW_H

#include <utility>
#include <type_traits>
#include <iterator>
#include <algorithm>

#include <QList>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QIODevice>
#include <QTextStream>
#include <QTemporaryFile>
#include <QGraphicsRectItem>
#include <QGraphicsPathItem>
#include <QPolygon>
#include <QGraphicsPolygonItem>
#include <QTimer>
#include <QPixmap>
#include <QMenu>

#include "traceitemview.h"
#include "callgraphview.h"
#include "tracedata.h"

class CFGEdge;
class CanvasCFGNode;

class CFGNode final
{
    template<typename It>
    using iterator_category_t = typename std::iterator_traits<It>::iterator_category;

    enum { noIndex, trueIndex, falseIndex };

public:
    using iterator = typename QStringList::iterator;
    using const_iterator = typename QStringList::const_iterator;
    using size_type = typename QStringList::size_type;

    CFGNode() = default;

    TraceBasicBlock* basicBlock() { return _bb; }
    const TraceBasicBlock* basicBlock() const { return _bb; }
    void setBasicBlock(TraceBasicBlock* bb) { _bb = bb; }

    CanvasCFGNode* canvasNode() { return _cn; }
    const CanvasCFGNode* canvasNode() const { return _cn; }
    void setCanvasNode(CanvasCFGNode *cn) { _cn = cn; }

    bool isVisible() const { return _visible; }
    void setVisible(bool v) { _visible = v; }

    void clearEdges();
    void sortPredecessorEdges();

    CFGEdge* trueEdge() { return _trueEdge; }
    const CFGEdge* trueEdge() const { return _trueEdge; }
    void setTrueEdge(CFGEdge*);

    CFGEdge* falseEdge() { return _falseEdge; }
    const CFGEdge* falseEdge() const { return _falseEdge; }
    void setFalseEdge(CFGEdge*);

    void selectSuccessorEdge(CFGEdge*);
    void selectPredecessorEdge(CFGEdge*);

    void addPredecessor(CFGEdge*);
    void addUniquePredecessor(CFGEdge*);

    double successorCostSum() const;
    double successorCountSum() const;

    double predecessorCostSum() const;
    double predecessorCountSum() const;

    // keyboard navigation
    CFGEdge* visibleSuccessorEdge();
    CFGEdge* visiblePredecessorEdge();

    CFGEdge* nextVisibleSuccessorEdge(CFGEdge* edge);
    CFGEdge* nextVisiblePredecessorEdge(CFGEdge* edge);

    CFGEdge* priorVisibleSuccessorEdge(CFGEdge* edge);
    CFGEdge* priorVisiblePredecessorEdge(CFGEdge* edge);

    template<typename It,
             typename = std::enable_if_t<std::is_base_of_v<std::forward_iterator_tag,
                                                           iterator_category_t<It>>>>
    void insertInstructions(It first, It last)
    {
        #ifdef TOTAL_DEBUG
        qDebug() << "\033[1;31m" << "CFGNode::insertInstructions" << "\033[0m";
        #endif // TOTAL_DEBUG

        assert(std::distance(first, last) == _bb->instrNumber());

        _instructions.reserve(std::distance(first, last));
        std::copy(first, last, std::back_inserter(_instructions));

        auto it = std::max_element(begin(), end(), [](QString& str1, QString& str2)
                                                   { return str1.length() < str2.length(); });

        _maxLength = it->length();
        _longestInstrIndex = std::distance(begin(), it);
    }

    const QString& longestInstr() const
    {
        assert (0 <= _longestInstrIndex && _longestInstrIndex < _instructions.size());
        return _instructions[_longestInstrIndex];
    }

    size_type instrNumber() const
    {
        assert (_instructions.size() == _bb->instrNumber());
        return _instructions.size();
    }

    iterator begin() { return _instructions.begin(); }
    const_iterator begin() const { return _instructions.begin(); }
    const_iterator cbegin() const { return begin(); }

    iterator end() { return _instructions.end(); }
    const_iterator end() const { return _instructions.end(); }
    const_iterator cend() const { return end(); }

    double self = 0.0;
    double incl = 0.0;

private:

    TraceBasicBlock* _bb = nullptr;

    CFGEdge* _trueEdge = nullptr;
    CFGEdge* _falseEdge = nullptr;
    int _lastSuccessorIndex = noIndex;

    QList<CFGEdge*> _predecessors;
    int _lastPredecessorIndex = -1;

    bool _lastFromPredecessor = true;

    bool _visible = false;
    CanvasCFGNode* _cn = nullptr;

    QStringList _instructions;
    int _maxLength;
    int _longestInstrIndex;
};


class CanvasCFGEdge;


class CFGEdge final
{
public:
    CFGEdge() = default;

    CanvasCFGEdge* canvasEdge() { return _ce; }
    const CanvasCFGEdge* canvasEdge() const { return _ce; }
    void setCanvasEdge(CanvasCFGEdge* ce) { _ce = ce; }

    bool isVisible() const { return _visible; }
    void setVisible(bool v) { _visible = v; }

    CFGNode* fromNode() { return _fromNode; }
    const CFGNode* fromNode() const { return _fromNode; }
    CFGNode* cachedFromNode();
    void setPredecessorNode(CFGNode* n) { _fromNode = n; }

    CFGNode* toNode() { return _toNode; }
    const CFGNode* toNode() const { return _toNode; }
    CFGNode* cachedToNode();
    void setSuccessorNode(CFGNode *n) { _toNode = n; }

    TraceBasicBlock* from();
    const TraceBasicBlock* from() const;
    TraceBasicBlock* cachedFrom();

    TraceBasicBlock* to();
    const TraceBasicBlock* to() const;
    TraceBasicBlock* cachedTo();

    CFGEdge* nextVisibleEdge();
    CFGEdge* priorVisibleEdge();

    QString prettyName() const;

    double cost = 0.0;
    double count = 0.0;

private:
    CFGNode* _fromNode = nullptr;
    CFGNode* _toNode = nullptr;

    CanvasCFGEdge* _ce = nullptr;
    bool _visible = false;

    bool _lastFromPredecessor = true;
};


class ControlFlowGraphView;


class CFGExporter final
{
public:
    using size_type = typename QMap<TraceBasicBlock*, CFGNode>::size_type;

    enum Layout {TopDown, LeftRight, Circular};
    enum DetailLevel { lessDetails, avgDetails, moreDetails };

    CFGExporter() = default;
    CFGExporter(TraceFunction* func, EventType* et, ProfileContext::Type gt,
                QString filename = QString{});
    ~CFGExporter();

    QString filename() const { return _dotName; }

    Layout layout() const { return _layout; }
    void setLayout(Layout layout) { _layout = layout; }

    DetailLevel detailLevel () const { return _detailLevel; }

    size_type edgeCount() const { return _edgeMap.count(); }
    size_type nodeCount() const { return _nodeMap.count(); }

    CFGNode* findNode(TraceBasicBlock* bb);
    const CFGNode* findNode(TraceBasicBlock* bb) const;

    CFGEdge* findEdge(TraceBasicBlock* bb1, TraceBasicBlock* bb2);
    const CFGEdge* findEdge(TraceBasicBlock* bb1, TraceBasicBlock* bb2) const;

    void reset(CostItem* i, EventType* et, ProfileContext::Type gt,
               QString filename = QString{});

    bool writeDot(QIODevice* device = nullptr);

    void sortEdges();

    // translates string "B<address>" into TraceBasicBlock* with value <address>
    static TraceBasicBlock* toBasicBlock(QString s);

    static bool savePrompt(QWidget *parent, TraceFunction *func,
                           EventType *eventType, ProfileContext::Type groupType);

private:
    bool createGraph();
    CFGNode* buildNode(TraceBasicBlock* bb);
    void buildEdge(CFGNode* fromNode, TraceBasicBlock* from, TraceBasicBlock* to);

    bool fillInstrStrings(TraceFunction* func);
    std::pair<QString, QString> runObjdump(TraceFunction* func, QProcess &objdump, bool isArm);

    void dumpLayoutSettings(QTextStream &ts);
    void dumpNodes(QTextStream &ts);
    void dumpEdges(QTextStream &ts);

    QString _dotName;
    QTemporaryFile* _tmpFile;

    CostItem* _item = nullptr;
    EventType* _eventType = nullptr;
    ProfileContext::Type _groupType = ProfileContext::InvalidType;

    bool _graphCreated = false;
    Layout _layout = Layout::TopDown;
    DetailLevel _detailLevel = avgDetails;

    QMap<TraceBasicBlock*, CFGNode> _nodeMap;
    QMap<std::pair<TraceBasicBlock*, TraceBasicBlock*>, CFGEdge> _edgeMap;
};

enum CanvasParts : int
{
    Node,
    Edge,
    EdgeLabel,
    EdgeArrow,
    Frame
};

class CanvasCFGNode : public QGraphicsRectItem, public StoredDrawParams
{
public:
    CanvasCFGNode(ControlFlowGraphView* view, CFGNode* node,
                  qreal x, qreal y, qreal w, qreal h);
    ~CanvasCFGNode() override = default;

    CFGNode* node() { return _node; }
    const CFGNode* node() const { return _node; };

    int type() const override { return CanvasParts::Node; }

    void updateGroup();
    void setSelected(bool s);
    void paint(QPainter*,
               [[maybe_unused]] const QStyleOptionGraphicsItem*,
               [[maybe_unused]] QWidget*) override;

private:
    CFGNode* _node;
    ControlFlowGraphView* _view;
};


class CanvasCFGEdgeLabel : public QGraphicsRectItem, public StoredDrawParams
{
public:
    CanvasCFGEdgeLabel(ControlFlowGraphView* v, CanvasCFGEdge* ce,
                       qreal x, qreal y, qreal w, qreal h);
    ~CanvasCFGEdgeLabel() override = default;

    CanvasCFGEdge* canvasEdge() { return _ce; }
    const CanvasCFGEdge* canvasEdge() const { return _ce; }

    int type() const override { return CanvasParts::EdgeLabel; }
    double percentage() const { return _percentage; }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

private:
    CanvasCFGEdge* _ce;
    ControlFlowGraphView* _view;

    double _percentage;
};


class CanvasCFGEdgeArrow : public QGraphicsPolygonItem
{
public:
    explicit CanvasCFGEdgeArrow(CanvasCFGEdge* e);

    CanvasCFGEdge* canvasEdge() { return _ce; }
    const CanvasCFGEdge* canvasEdge() const { return _ce; }

    int type() const override { return CanvasParts::EdgeArrow; }

    void paint(QPainter* p,
               [[maybe_unused]] const QStyleOptionGraphicsItem*,
               [[maybe_unused]] QWidget*) override;

private:
    CanvasCFGEdge* _ce;
};


class CanvasCFGEdge : public QGraphicsPathItem
{
public:
    explicit CanvasCFGEdge(CFGEdge *e);

    CanvasCFGEdgeLabel* label() { return _label; }
    const CanvasCFGEdgeLabel* label() const { return _label; }
    void setLabel(CanvasCFGEdgeLabel *l);

    CanvasCFGEdgeArrow* arrow() { return _arrow; }
    const CanvasCFGEdgeArrow* arrow() const { return _arrow; }
    void setArrow(CanvasCFGEdgeArrow *a);

    const QPolygon& controlPoints() const { return _points; }
    void setControlPoints(const QPolygon &p);

    CFGEdge* edge() { return _edge; }
    const CFGEdge* edge() const { return _edge; }

    int type() const override { return CanvasParts::Edge; }

    void setSelected(bool s);

    void paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget*) override;

private:
    CFGEdge* _edge;
    CanvasCFGEdgeLabel* _label = nullptr;
    CanvasCFGEdgeArrow* _arrow = nullptr;
    QPolygon _points;

    double _thickness = 0.0;
};


class CanvasCFGFrame : public QGraphicsRectItem
{
public:
    explicit CanvasCFGFrame(CanvasCFGNode*);

    int type() const override { return CanvasParts::Frame; }
    bool hit(const QPoint&) const { return false; }

    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

private:
    static QPixmap* _p;
};


class ControlFlowGraphView : public QGraphicsView, public TraceItemView,
                             public StorableGraphOptions
{
    Q_OBJECT

public:
    enum class ZoomPosition
    {
        TopLeft, TopRight,
        BottomLeft, BottomRight,
        Auto, Hide
    };

    explicit ControlFlowGraphView(TraceItemView* parentView, QWidget* parent,
                                  const QString &name);
    ~ControlFlowGraphView() override;

    QWidget* widget() override { return this; }
    ZoomPosition zoomPos () const { return _zoomPosition; }

    QString whatsThis() const override;

public Q_SLOTS:
    void zoomRectMoved(qreal, qreal);
    void zoomRectMoveFinished();

    void showRenderWarning();
    void showRenderError(const QString &s);
    void stopRendering();
    void readDotOutput();
    void dotError();
    void dotExited();
#if 0
    void predecessorDepthTriggered(QAction*);
    void successorDepthTriggered(QAction*);
    void nodeLimitTriggered(QAction*);
    void branchLimitTriggered(QAction*);
#endif
    void zoomPosTriggered(QAction*);
    void layoutTriggered(QAction*);

protected:
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent([[maybe_unused]] QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void mouseEvent(void (TraceItemView::* func)(CostItem*), QGraphicsItem* item);
    void updateSizes(QSize s = QSize(0,0));
    CostItem* canShow(CostItem*) override;
    void doUpdate(int changeType, bool) override;
    void resetOldSelectedNodeAndEdge();
    void setNewSelectedNodeAndEdge(CFGNode* node, CFGEdge* edge);
    void refresh();
    void clear();
    void showText(const QString &);

    // called from dotExited
    std::pair<CFGNode*, CFGEdge*> parseDot();
    std::pair<double, double> calculateScales(QTextStream& ts);
    double setupScreen(QTextStream &ts, double scaleX, double scaleY, int lineno);
    std::pair<int, int> calculateSizes(QTextStream &ts, double scaleX, double scaleY,
                                                                       double dotHeight);
    CFGNode *parseNode(CFGNode* activeNode, QTextStream &ts, double scaleX,
                       double scaleY, double dotHeight);
    CFGEdge* parseEdge(CFGEdge* activeEdge, QTextStream &ts, double scaleX,
                       double scaleY, double dotHeight, int lineno);
    void checkSceneAndActiveItems(CFGNode* activeNode, CFGEdge* activeEdge);
    void updateSelectedNodeOrEdge(CFGNode* activeNode, CFGEdge* activeEdge);
    void centerOnSelectedNodeOrEdge();

    // called from keyPressEvent
    int transformKeyIfNeeded(int key);
    std::pair<CFGNode*, CFGEdge*> getNodeAndEdgeToSelect(int key);
    void movePointOfView(QKeyEvent* e);

    QAction* addPredecessorDepthAction(QMenu*, QString, int);
    QAction* addSuccessorDepthAction(QMenu*, QString, int);
    QAction* addNodeLimitAction(QMenu*, QString, double);
    QAction* addBranchLimitAction(QMenu*, QString, double);
    QAction* addZoomPosAction(QMenu*, QString, ControlFlowGraphView::ZoomPosition);
    QAction* addLayoutAction(QMenu*, QString, CFGExporter::Layout);

    QMenu* addPredecessorDepthMenu(QMenu*);
    QMenu* addSuccessorDepthMenu(QMenu*);
    QMenu* addNodeLimitMenu(QMenu*);
    QMenu* addBranchLimitMenu(QMenu*);
    QMenu* addZoomPosMenu(QMenu*);
    QMenu* addLayoutMenu(QMenu*);

    QGraphicsScene* _scene = nullptr;
    PanningView* _panningView;
    double _panningZoom = 1.0;
    int _xMargin = 0;
    int _yMargin = 0;

    bool _isMoving = false;
    QPoint _lastPos;

    CFGExporter _exporter;

    CFGNode* _selectedNode = nullptr;
    CFGEdge* _selectedEdge = nullptr;

    ZoomPosition _zoomPosition = ZoomPosition::Auto;
    ZoomPosition _lastAutoPosition = ZoomPosition::TopLeft;

    // background rendering
    QProcess* _renderProcess = nullptr;
    QString _renderProcessCmdLine;
    QTimer _renderTimer;
    CFGNode* _prevSelectedNode = nullptr;
    QPoint _prevSelectedPos;
    QString _unparsedOutput;
};

#endif

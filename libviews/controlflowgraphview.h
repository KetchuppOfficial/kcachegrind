#ifndef CONTROLFLOWGRAPHVIEW_H
#define CONTROLFLOWGRAPHVIEW_H

#include <utility>
#include <type_traits>
#include <iterator>
#include <algorithm>
#include <optional>
#include <variant>

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

public:
    using iterator = typename QStringList::iterator;
    using const_iterator = typename QStringList::const_iterator;
    using size_type = typename QStringList::size_type;

    CFGNode(TraceBasicBlock* bb);

    TraceBasicBlock* basicBlock() { return _bb; }
    const TraceBasicBlock* basicBlock() const { return _bb; }

    CanvasCFGNode* canvasNode() { return _cn; }
    const CanvasCFGNode* canvasNode() const { return _cn; }
    void setCanvasNode(CanvasCFGNode* cn) { _cn = cn; }

    bool isVisible() const { return _visible; }
    void setVisible(bool v) { _visible = v; }

    void addSuccessorEdge(CFGEdge*);
    void addPredecessorEdge(CFGEdge*);

    void clearEdges();
    void sortSuccessorEdges();
    void sortPredecessorEdges();

    double successorCostSum() const;
    double successorCountSum() const;

    double predecessorCostSum() const;
    double predecessorCountSum() const;

    // keyboard navigation
    void selectSuccessorEdge(CFGEdge*);
    void selectPredecessorEdge(CFGEdge*);

    CFGEdge* keyboardNextEdge();
    CFGEdge* keyboardPrevEdge();

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

        using diff_type = typename std::iterator_traits<It>::difference_type;

        assert(std::distance(first, last) == static_cast<diff_type>(_bb->instrNumber()));

        _instructions.reserve(std::distance(first, last));
        std::copy(first, last, std::back_inserter(_instructions));
    }

    size_type instrNumber() const
    {
        assert (_instructions.size() == static_cast<qsizetype>(_bb->instrNumber()));
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

    TraceBasicBlock* _bb;

    QList<CFGEdge*> _successors;
    int _lastSuccessorIndex = -1;

    QList<CFGEdge*> _predecessors;
    int _lastPredecessorIndex = -1;

    bool _visible = false;
    CanvasCFGNode* _cn = nullptr;

    QStringList _instructions;
};


class CanvasCFGEdge;


class CFGEdge final
{
public:
    enum NodeType { none, nodeTo_, nodeFrom_ };

    CFGEdge(TraceBranch* branch);

    CanvasCFGEdge* canvasEdge() { return _ce; }
    const CanvasCFGEdge* canvasEdge() const { return _ce; }
    void setCanvasEdge(CanvasCFGEdge* ce) { _ce = ce; }

    TraceBranch* branch() { return _branch; }
    const TraceBranch* branch() const { return _branch; }

    CFGNode* nodeFrom() { return _nodeFrom; }
    const CFGNode* nodeFrom() const { return _nodeFrom; }
    void setNodeFrom(CFGNode* node) { _nodeFrom = node; }

    CFGNode* nodeTo() { return _nodeTo; }
    const CFGNode* nodeTo() const { return _nodeTo; }
    void setNodeTo(CFGNode* node) { _nodeTo = node; }

    bool isVisible() const { return _visible; }
    void setVisible(bool v) { _visible = v; }

    TraceBasicBlock* bbFrom();
    const TraceBasicBlock* bbFrom() const;

    TraceBasicBlock* bbTo();
    const TraceBasicBlock* bbTo() const;

    CFGNode* keyboardNextNode();
    CFGNode* keyboardPrevNode();
    CFGEdge* nextVisibleEdge();
    CFGEdge* priorVisibleEdge();

    NodeType visitedFrom() const { return _visitedFrom; }
    void setVisitedFrom(NodeType node) { _visitedFrom = node; }

    double cost = 0.0;
    double count = 0.0;

private:

    TraceBranch* _branch;

    CFGNode* _nodeFrom = nullptr;
    CFGNode* _nodeTo = nullptr;

    CanvasCFGEdge* _ce = nullptr;
    bool _visible = false;

    NodeType _visitedFrom = NodeType::none;
};


class ControlFlowGraphView;


class CFGExporter final
{
public:
    using size_type = typename QMap<TraceBasicBlock*, CFGNode>::size_type;

    enum Layout { TopDown, LeftRight };
    enum DetailsLevel { pcOnly, full };
    enum DumpType { internal, external };

    using details_map_type = std::unordered_map<TraceBasicBlock*, DetailsLevel>;

    CFGExporter() = default;
    CFGExporter(TraceFunction* func, EventType* et, ProfileContext::Type gt,
                QString filename = QString{});
    ~CFGExporter();

    QString filename() const { return _dotName; }

    bool CFGAvailable() const { return _errorMessage.isEmpty(); }
    const QString& errorMessage() const { return _errorMessage; }

    Layout layout() const { return _layout; }
    void setLayout(Layout layout) { _layout = layout; }

    size_type edgeCount() const { return _edgeMap.count(); }
    size_type nodeCount() const { return _nodeMap.count(); }

    DetailsLevel detailsLevel(TraceBasicBlock* bb) const;
    void setDetailsLevel(TraceBasicBlock* bb, DetailsLevel level);
    void switchDetailsLevel(TraceBasicBlock* bb);
    void setDetailsLevel(TraceFunction* func, DetailsLevel level);
    void minimizeBBsWithCostLessThan(uint64 minimalCost);
    int minimalCostPercentage() const { return _minimalCostPercentage; }
    void setMinimalCostPercentage(int p) { _minimalCostPercentage = p; }

    CFGNode* findNode(TraceBasicBlock* bb);
    const CFGNode* findNode(TraceBasicBlock* bb) const;

    CFGEdge* findEdge(Addr from, Addr to);
    const CFGEdge* findEdge(Addr from, Addr to) const;

    void reset(CostItem* i, EventType* et, ProfileContext::Type gt,
               QString filename = QString{});

    bool writeDot(DumpType type, QIODevice* device = nullptr);

    void sortEdges();

    int transformKeyIfNeeded(int key) const;

    // translates string "B<firstAddr>B<lastAddr>" into appropriate CFGNode*
    CFGNode* toCFGNode(QString s);

    details_map_type& detailsMap() { return _detailsMap; }
    const details_map_type& detailsMap() const { return _detailsMap; }
    void setDetailsMap(const details_map_type& map) { _detailsMap = map; }

    static bool savePrompt(QWidget* parent, TraceFunction* func,
                           EventType* eventType, ProfileContext::Type groupType,
                           Layout layout, const details_map_type& map);

private:
    bool createGraph();
    CFGNode* buildNode(TraceBasicBlock* bb);
    CFGEdge* buildEdge(CFGNode* nodeFrom, TraceBranch* branch);
    void addPredecessors();

    bool fillInstrStrings(TraceFunction* func);

    void dumpNodes(QTextStream& ts);
    void dumpEdges(QTextStream& ts, DumpType type);

    QString _dotName;
    QTemporaryFile* _tmpFile;

    CostItem* _item = nullptr;
    EventType* _eventType = nullptr;
    ProfileContext::Type _groupType = ProfileContext::InvalidType;

    QString _errorMessage;

    bool _graphCreated = false;
    Layout _layout = Layout::TopDown;

    QMap<std::pair<Addr, Addr>, CFGNode> _nodeMap;
    QMap<std::pair<Addr, Addr>, CFGEdge> _edgeMap;

    details_map_type _detailsMap;
    int _minimalCostPercentage = -1;
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
    explicit CanvasCFGEdge(CFGEdge* e);

    CanvasCFGEdgeLabel* label() { return _label; }
    const CanvasCFGEdgeLabel* label() const { return _label; }
    void setLabel(CanvasCFGEdgeLabel* l);

    CanvasCFGEdgeArrow* arrow() { return _arrow; }
    const CanvasCFGEdgeArrow* arrow() const { return _arrow; }
    void setArrow(CanvasCFGEdgeArrow* a);

    const QPolygon& controlPoints() const { return _points; }
    void setControlPoints(const QPolygon& p);

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

    explicit ControlFlowGraphView(TraceItemView* parentView, QWidget* parent, const QString& name);
    ~ControlFlowGraphView() override;

    QWidget* widget() override { return this; }
    ZoomPosition zoomPos() const { return _zoomPosition; }

    QString whatsThis() const override;

    bool isReduced(CFGNode* node) const;
    TraceFunction* getFunction();

public Q_SLOTS:
    void zoomRectMoved(qreal, qreal);
    void zoomRectMoveFinished();

    void showRenderWarning();
    void showRenderError(const QString& s);
    void stopRendering();
    void readDotOutput();
    void dotError();
    void dotExited();
    void zoomPosTriggered(QAction*);
    void layoutTriggered(QAction*);
    void minimizationTriggered(QAction*);

protected:
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent([[maybe_unused]] QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void exportGraphAsImage();
    void keyPressEvent(QKeyEvent*) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    void mouseEvent(void (TraceItemView::* func)(CostItem*), QGraphicsItem* item);
    void updateSizes(QSize s = QSize(0,0));
    CostItem* canShow(CostItem*) override;
    void doUpdate(int changeType, bool) override;
    void unselectNode();
    void unselectEdge();
    void selectNode(CFGNode* node);
    void selectEdge(CFGEdge* edge);
    void refresh(bool reset = true);
    void clear();
    void showText(const QString &);

    // called from dotExited
    void parseDot();
    void setupScreen(QTextStream& lineStream, int lineno);
    std::pair<int, int> calculateSizes(QTextStream& lineStream);
    void parseNode(QTextStream& lineStream);
    void parseEdge(QTextStream& lineStream, int lineno);
    CFGEdge* getEdgeFromDot(QTextStream& lineStream, int lineno);
    QPolygon getEdgePolygon(QTextStream& lineStream, int lineno);
    void checkScene();
    void updateSelectedNodeOrEdge(CFGNode* activeNode, CFGEdge* activeEdge);
    void centerOnSelectedNodeOrEdge();

    // called from keyPressEvent
    void movePointOfView(QKeyEvent* e);

    QAction* addZoomPosAction(QMenu* m, const QString& descr, ZoomPosition pos);
    QAction* addLayoutAction(QMenu* m, const QString& descr, CFGExporter::Layout);
    QAction* addStopLayoutAction(QMenu&);
    QAction* addDetailsAction(QMenu* m, const QString& descr, CFGNode* node,
                                                CFGExporter::DetailsLevel level);
    QAction* addMinimizationAction(QMenu* m, const QString& descr, int percentage);

    QMenu* addZoomPosMenu(QMenu& menu);
    QMenu* addLayoutMenu(QMenu& menu);
    QMenu* addMinimizationMenu(QMenu& menu);

    QGraphicsScene* _scene = nullptr;
    QPoint _lastPos;
    static constexpr double _scaleX = 80.0;
    double _scaleY;
    double _dotHeight = 0.0;
    int _xMargin = 0;
    int _yMargin = 0;
    bool _isMoving = false;

    PanningView* _panningView;
    double _panningZoom = 1.0;
    ZoomPosition _zoomPosition = ZoomPosition::Auto;
    ZoomPosition _lastAutoPosition = ZoomPosition::TopLeft;

    CFGExporter _exporter;

    CFGNode* _selectedNode = nullptr;
    CFGNode* _prevSelectedNode = nullptr;
    CFGEdge* _selectedEdge = nullptr;
    QPoint _prevSelectedPos;

    QProcess* _renderProcess = nullptr;
    QString _renderProcessCmdLine;
    QTimer _renderTimer;
    QString _unparsedOutput;
};

#endif

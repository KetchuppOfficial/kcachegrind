#include <numeric>
#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <cmath>
#include <array>
#include <cstring>

#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QRectF>
#include <QRect>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QSize>
#include <QTransform>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QApplication>
#include <QScreen>

#include "controlflowgraphview.h"
#include "globalconfig.h"
#include "globalguiconfig.h"
#include "listutils.h"

// ======================================================================================

//
// CFGNode
//

CFGNode::CFGNode(TraceBasicBlock* bb) : _bb{bb} {}

void CFGNode::addSuccessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::setSuccessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (edge)
    {
        _successors.append(edge);
        edge->setNodeFrom(this);
    }
}

void CFGNode::addPredecessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::addPredecessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (edge)
        _predecessors.append(edge);
}

void CFGNode::clearEdges()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::clearEdges()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    _predecessors.clear();
    _successors.clear();
}

void CFGNode::sortSuccessorEdges()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::sortSuccessorEdges()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    auto edgeComp = [canvasNode = _cn](const CFGEdge* ge1, const CFGEdge* ge2)
    {
        const CanvasCFGEdge* ce1 = ge1->canvasEdge();
        const CanvasCFGEdge* ce2 = ge2->canvasEdge();

        if (!ce1 && !ce2)
            return ce1 > ce2;
        else if (!ce1)
            return false;
        else if (!ce2)
            return true;
        else
        {
            QPointF center = canvasNode->rect().bottomLeft();

            QPointF d1 = ce1->controlPoints().back() - center;
            QPointF d2 = ce2->controlPoints().back() - center;

            qreal angle1 = std::atan2(d1.y(), d1.x());
            qreal angle2 = std::atan2(d2.y(), d2.x());

            return angle1 > angle2;
        }
    };

    if (!_successors.empty() && _successors[0]->branch()->brType() == TraceBranch::Type::indirect)
        std::sort(_successors.begin(), _successors.end(), edgeComp);
}

void CFGNode::sortPredecessorEdges()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::sortPredecessorEdges()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    auto edgeComp = [canvasNode = _cn](const CFGEdge* ge1, const CFGEdge* ge2)
    {
        const CanvasCFGEdge* ce1 = ge1->canvasEdge();
        const CanvasCFGEdge* ce2 = ge2->canvasEdge();

        if (!ce1 && !ce2)
            return ce1 > ce2;
        else if (!ce1)
            return false;
        else if (!ce2)
            return true;
        else
        {
            QRectF nodeRect = canvasNode->rect();
            QPointF center = nodeRect.center();
            center.setY(nodeRect.bottom());

            QPointF d1 = ce1->controlPoints().back() - center;
            QPointF d2 = ce2->controlPoints().back() - center;

            /* y coordinate is negated to change orientation of the coordinate system
               from positive to negative */
            qreal angle1 = std::atan2(-d1.y(), d1.x());
            qreal angle2 = std::atan2(-d2.y(), d2.x());

            return angle1 > angle2;
        }
    };

    std::sort(_predecessors.begin(), _predecessors.end(), edgeComp);
}

void CFGNode::selectSuccessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::selectSuccessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    _lastSuccessorIndex = _successors.indexOf(edge);
}

void CFGNode::selectPredecessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::selectPredecessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    _lastPredecessorIndex = _predecessors.indexOf(edge);
}

CFGEdge* CFGNode::keyboardNextEdge()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::keyboardNextEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    CFGEdge* edge = _successors.value(_lastSuccessorIndex);

    if (edge && !edge->isVisible())
        edge = nullptr;

    if (edge)
        edge->setVisitedFrom(CFGEdge::NodeType::nodeFrom_);
    else if (!_successors.isEmpty())
    {
        CFGEdge* maxEdge = _successors[0];
        double maxCount = maxEdge->count;

        for (decltype(_successors.size()) i = 1; i < _successors.size(); ++i)
        {
            edge = _successors[i];

            if (edge->isVisible() && edge->count > maxCount)
            {
                maxEdge = edge;
                maxCount = maxEdge->count;
                _lastSuccessorIndex = i;
            }
        }

        edge = maxEdge;
        edge->setVisitedFrom(CFGEdge::NodeType::nodeFrom_);
    }

    return edge;
}

CFGEdge* CFGNode::keyboardPrevEdge()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::keyboardPrevEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    CFGEdge* edge = _predecessors.value(_lastPredecessorIndex);

    if (edge && !edge->isVisible())
        edge = nullptr;

    if (edge)
        edge->setVisitedFrom(CFGEdge::NodeType::nodeTo_);
    else if (!_predecessors.isEmpty())
    {
        CFGEdge* maxEdge = _predecessors[0];
        double maxCount = maxEdge->count;

        for (decltype(_predecessors.size()) i = 1; i < _predecessors.size(); ++i)
        {
            edge = _predecessors[i];

            if (edge->isVisible() && edge->count > maxCount)
            {
                maxEdge = edge;
                maxCount = maxEdge->count;
                _lastPredecessorIndex = i;
            }
        }

        edge = maxEdge;
        edge->setVisitedFrom(CFGEdge::NodeType::nodeTo_);
    }

    return edge;
}

CFGEdge* CFGNode::nextVisibleSuccessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::nextVisibleSuccessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    int shift = edge ? _successors.indexOf(edge) : _lastSuccessorIndex;
    auto begin = std::next(_successors.begin(), shift + 1);
    auto end = _successors.end();

    auto it = std::find_if(begin, end, [](CFGEdge* e){ return e->isVisible(); });

    if (it == end)
        return nullptr;
    else
    {
        _lastSuccessorIndex = std::distance(_successors.begin(), it);
        return *it;
    }
}

CFGEdge* CFGNode::nextVisiblePredecessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::nextVisiblePredecessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    int shift = edge ? _predecessors.indexOf(edge) : _lastPredecessorIndex;
    auto begin = std::next(_predecessors.begin(), shift + 1);
    auto end = _predecessors.end();

    auto it = std::find_if(begin, end, [](CFGEdge* e){ return e->isVisible(); });

    if (it == end)
        return nullptr;
    else
    {
        _lastPredecessorIndex = std::distance(_predecessors.begin(), it);
        return *it;
    }
}

CFGEdge* CFGNode::priorVisibleSuccessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::priorVisibleSuccessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    int idx = edge ? _successors.indexOf(edge) : _lastSuccessorIndex;

    idx = (idx < 0) ? _successors.size() - 1 : idx - 1;
    for (; idx >= 0; --idx)
    {
        edge = _successors[idx];
        if (edge->isVisible())
        {
            _lastSuccessorIndex = idx;
            return edge;
        }
    }

    return nullptr;
}

CFGEdge* CFGNode::priorVisiblePredecessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::priorVisiblePredecessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    int idx = edge ? _predecessors.indexOf(edge) : _lastPredecessorIndex;

    idx = (idx < 0) ? _predecessors.size() - 1 : idx - 1;
    for (; idx >= 0; --idx)
    {
        edge = _predecessors[idx];
        if (edge->isVisible())
        {
            _lastPredecessorIndex = idx;
            return edge;
        }
    }

    return nullptr;
}

// ======================================================================================

//
// CFGEdge
//

CFGEdge::CFGEdge(TraceBranch* branch) : _branch{branch} {}

TraceBasicBlock* CFGEdge::bbFrom()
{
    return _nodeFrom ? _nodeFrom->basicBlock() : nullptr;
}

const TraceBasicBlock* CFGEdge::bbFrom() const
{
    return _nodeFrom ? _nodeFrom->basicBlock() : nullptr;
}

TraceBasicBlock* CFGEdge::bbTo()
{
    return _nodeTo ? _nodeTo->basicBlock() : nullptr;
}

const TraceBasicBlock* CFGEdge::bbTo() const
{
    return _nodeTo ? _nodeTo->basicBlock() : nullptr;
}

CFGNode* CFGEdge::keyboardNextNode()
{
    if (_nodeTo)
        _nodeTo->selectPredecessorEdge(this);

    return _nodeTo;
}

CFGNode* CFGEdge::keyboardPrevNode()
{
    if (_nodeFrom)
        _nodeFrom->selectSuccessorEdge(this);

    return _nodeFrom;
}

CFGEdge* CFGEdge::nextVisibleEdge()
{
    if (_visitedFrom == NodeType::nodeTo_)
    {
        assert(_nodeTo);

        CFGEdge* edge = _nodeTo->nextVisiblePredecessorEdge(this);
        if (edge)
            edge->setVisitedFrom(NodeType::nodeTo_);

        return edge;
    }
    else if (_visitedFrom == NodeType::nodeFrom_)
    {
        assert(_nodeFrom);

        CFGEdge* edge = _nodeFrom->nextVisibleSuccessorEdge(this);
        if (edge)
            edge->setVisitedFrom(NodeType::nodeFrom_);

        return edge;
    }
    else
        return nullptr;
}

CFGEdge* CFGEdge::priorVisibleEdge()
{
    if (_visitedFrom == NodeType::nodeTo_)
    {
        assert(_nodeTo);

        CFGEdge* edge = _nodeTo->priorVisiblePredecessorEdge(this);
        if (edge)
            edge->setVisitedFrom(NodeType::nodeTo_);

        return edge;
    }
    else if (_visitedFrom == NodeType::nodeFrom_)
    {
        assert(_nodeFrom);

        CFGEdge* edge = _nodeFrom->priorVisibleSuccessorEdge(this);
        if (edge)
            edge->setVisitedFrom(NodeType::nodeFrom_);

        return edge;
    }
    else
        return nullptr;
}

// ======================================================================================

//
// CFGExporter
//

CFGExporter::CFGExporter(TraceFunction* func, EventType* et, ProfileContext::Type gt,
                         QString filename)
                        : _item{func}, _eventType{et}, _groupType{gt}
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::CFGExporter()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (!_item)
        return;

    if (filename.isEmpty())
    {
        _tmpFile = new QTemporaryFile{};
        _tmpFile->setAutoRemove(false);
        _tmpFile->open();
        _dotName = _tmpFile->fileName();
    }
    else
    {
        _tmpFile = nullptr;
        _dotName = filename;
    }

    auto& basicBlocks = func->basicBlocks();
    assert(!basicBlocks.empty());
    _detailsMap.reserve(basicBlocks.size());
    for (auto bb : basicBlocks)
        _detailsMap.emplace(bb, DetailsLevel::full);
}

CFGExporter::~CFGExporter()
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::~CFGExporter()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    delete _tmpFile;
}

CFGExporter::DetailsLevel CFGExporter::detailsLevel(TraceBasicBlock* bb) const
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::detailsLevel()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    auto it = _detailsMap.find(bb);
    assert(it != _detailsMap.end());
    return it->second;
}

void CFGExporter::setDetailsLevel(TraceBasicBlock* bb, DetailsLevel level)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::setDetailsLevel()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    auto it = _detailsMap.find(bb);
    if (it != _detailsMap.end())
        it->second = level;
}

void CFGExporter::switchDetailsLevel(TraceBasicBlock* bb)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::switchDetailsLevel()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    auto it = _detailsMap.find(bb);
    if (it != _detailsMap.end())
    {
        if (it->second == DetailsLevel::pcOnly)
            it->second = DetailsLevel::full;
        else
            it->second = DetailsLevel::pcOnly;
    }
}

void CFGExporter::setDetailsLevel(TraceFunction* func, DetailsLevel level)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::setDetailsLevel()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    assert(func);
    auto& BBs = func->basicBlocks();
    for (auto bb : BBs)
        _detailsMap[bb] = level;
}

void CFGExporter::minimizeBBsWithCostLessThan(uint64 minimalCost)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::minimizeBBsWithCostLessThan()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    for (auto& node : _nodeMap)
    {
        if (node.self <= minimalCost)
            _detailsMap[node.basicBlock()] = DetailsLevel::pcOnly;
        else
            _detailsMap[node.basicBlock()] = DetailsLevel::full;
    }
}

const CFGNode* CFGExporter::findNode(TraceBasicBlock* bb) const
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::findNode()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (!bb)
        return nullptr;

    auto it = _nodeMap.find(std::make_pair(bb->firstAddr(), bb->lastAddr()));
    return (it == _nodeMap.end()) ? nullptr : std::addressof(*it);
}

CFGNode* CFGExporter::findNode(TraceBasicBlock* bb)
{
    return const_cast<CFGNode*>(static_cast<const CFGExporter*>(this)->findNode(bb));
}

const CFGEdge* CFGExporter::findEdge(Addr from, Addr to) const
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::findEdge()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    auto it = _edgeMap.find(std::make_pair(from, to));
    return (it == _edgeMap.end()) ? nullptr : std::addressof(*it);
}

CFGEdge* CFGExporter::findEdge(Addr from, Addr to)
{
    return const_cast<CFGEdge*>(static_cast<const CFGExporter*>(this)->findEdge(from, to));
}

void CFGExporter::reset(CostItem* i, EventType* et, ProfileContext::Type gt, QString filename)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::reset()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    _graphCreated = false;

    _eventType = et;
    _groupType = gt;

    _nodeMap.clear();
    _edgeMap.clear();

    if (_item && _tmpFile)
    {
        _tmpFile->setAutoRemove(true);
        delete _tmpFile;
    }

    if (i)
    {
        QString message = QObject::tr("Control-flow graph requires running "
                                      "callgrind with option --dump-instr=yes");
        switch (i->type())
        {
            case ProfileContext::Function:
            {
                auto func = static_cast<TraceFunction*>(i);
                auto& BBs = func->basicBlocks();
                if (BBs.empty())
                {
                    _errorMessage = message;
                    return;
                }

                _item = BBs.front();

                for (auto bb : BBs)
                    _detailsMap.emplace(bb, DetailsLevel::full);

                break;
            }
            case ProfileContext::Call:
            {
                auto caller = static_cast<TraceCall*>(i)->caller(true);
                auto& BBs = caller->basicBlocks();
                if (BBs.empty())
                {
                    _errorMessage = message;
                    return;
                }

                _item = BBs.front();

                break;
            }
            case ProfileContext::BasicBlock:
                _item = i;
                break;
            default: // we ignore function cycles
                _item = nullptr;
                return;
        }

        if (filename.isEmpty())
        {
            _tmpFile = new QTemporaryFile{};
            _tmpFile->setAutoRemove(false);
            _tmpFile->open();
            _dotName = _tmpFile->fileName();
        }
        else
        {
            _tmpFile = nullptr;
            _dotName = filename;
        }
    }
    else
    {
        _item = nullptr;
        _dotName.clear();
    }
}

void CFGExporter::sortEdges()
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::sortEdges()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    for (auto& node : _nodeMap)
    {
        node.sortPredecessorEdges();
        node.sortSuccessorEdges();
    }
}

bool CFGExporter::writeDot(DumpType type, QIODevice* device)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::writeDot()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (!_item)
        return false;

    // copy-constructors of QFile and QTextStream are deleted
    // so we have to allocate them on heap
    QFile* file = nullptr;
    QTextStream* stream = nullptr;

    if (device)
        stream = new QTextStream{device};
    else if (_tmpFile)
        stream = new QTextStream{_tmpFile};
    else
    {
        file = new QFile{_dotName};
        if (!file->open(QIODevice::WriteOnly))
        {
            qDebug() << "Cannot write dot file \'" << _dotName << "\'";
            delete file;
            return false;
        }
        stream = new QTextStream{file};
    }

    if (_graphCreated || createGraph())
    {
        *stream << "digraph \"control-flow graph\" {\n";

        if (_layout == Layout::LeftRight)
            *stream << QStringLiteral("  rankdir=LR;\n");

        dumpNodes(*stream);
        dumpEdges(*stream, type);

        *stream << "}\n";
    }

    if (!device)
    {
        if (_tmpFile)
        {
            stream->flush();
            _tmpFile->seek(0);
        }
        else
            delete file;
    }

    delete stream;

    return true;
}

bool CFGExporter::createGraph()
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::createGraph()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (!_item || _graphCreated)
        return false;

    _graphCreated = true;

    TraceFunction* func;
    switch(_item->type())
    {
        case ProfileContext::Function:
            func = static_cast<TraceFunction*>(_item);
            break;
        case ProfileContext::Call:
            func = static_cast<TraceCall*>(_item)->caller(false);
            break;
        case ProfileContext::BasicBlock:
            func = static_cast<TraceBasicBlock*>(_item)->function();
            break;
        default:
            assert(!"Unsupported type of item");
    }

    auto& BBs = func->basicBlocks();
    for (auto bb : BBs)
    {
        auto nodeIt = _nodeMap.insert(std::make_pair(bb->firstAddr(), bb->lastAddr()), CFGNode{bb});
        nodeIt->self = bb->subCost(_eventType);
    }

    for (auto& node : _nodeMap)
    {
        TraceBasicBlock* bbFrom = node.basicBlock();
        TraceBasicBlock::size_type nBranches = bbFrom->nBranches();

        for (decltype(nBranches) i = 0; i != nBranches; ++i)
        {
            TraceBranch& br = bbFrom->branch(i);
            TraceInstr* instrTo = br.instrTo();
            TraceBasicBlock* bbTo = instrTo->basicBlock();

            CFGEdge edge{std::addressof(br)};
            edge.setNodeFrom(std::addressof(node));
            edge.setNodeTo(findNode(bbTo));
            edge.count = br.executedCount();

            std::pair key{br.instrFrom()->addr(), instrTo->addr()};
            auto edgeIt = _edgeMap.insert(key, edge);
            node.addSuccessorEdge(std::addressof(*edgeIt));
        }
    }

    for (auto &node : _nodeMap)
        for (auto branch : node.basicBlock()->predecessors())
            node.addPredecessorEdge(findEdge(branch->instrFrom()->addr(),
                                             branch->instrTo()->addr()));

    return fillInstrStrings(func);
}

int CFGExporter::transformKeyIfNeeded(int key) const
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::transformKeyIfNeeded()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (_layout == Layout::LeftRight)
    {
        switch (key)
        {
            case Qt::Key_Up:
                key = Qt::Key_Left;
                break;
            case Qt::Key_Down:
                key = Qt::Key_Right;
                break;
            case Qt::Key_Left:
                key = Qt::Key_Up;
                break;
            case Qt::Key_Right:
                key = Qt::Key_Down;
                break;
            default:
                assert(false); // we should never reach here
        }
    }

    return key;
}

namespace
{

class LineBuffer final
{
public:
    using pos_type = std::size_t;

    LineBuffer() = default;

    pos_type capacity() const { return _bufSize; }

    pos_type getPos() const { return _pos; }
    void setPos(pos_type pos) { _pos = pos; }

    char elem(pos_type offset = 0) const { return _buf[_pos + offset]; }
    void setElem(pos_type pos, char c) { _buf[pos] = c; }

    // relatively to _pos
    char* relData(pos_type offset = 0) { return _buf + _pos + offset; }
    const char* relData(pos_type offset = 0) const { return _buf + _pos + offset; }

    // relatively to the beginning of _buf
    char* absData(pos_type offset = 0) { return _buf + offset; }
    const char* absData(pos_type offset = 0) const { return _buf + offset; }

    void advance(pos_type offset) { _pos += offset; }

    void skipWhitespaces()
    {
        while (_buf[_pos] == ' ' || _buf[_pos] == '\t')
            _pos++;
    }

private:
    static constexpr pos_type _bufSize = 256;
    char _buf[_bufSize];
    pos_type _pos = 0;
};

class ObjdumpParser final
{
public:
    ObjdumpParser(TraceFunction* func);
    ~ObjdumpParser() = default;

    std::pair<QString, QMap<Addr, std::pair<QString, QString>>> getInstrStrings();

private:
    using instr_iterator = typename TraceInstrMap::iterator;

    bool runObjdump(TraceFunction* func);
    bool searchFile(QString& dir, TraceObject* o, TraceData* data);
    QString getObjDump();
    QString getObjDumpFormat();
    QString getSysRoot();

    static bool isHexDigit(char c);

    Addr parseAddress();
    QString parseEncoding();
    QString parseMnemonic();
    QString parseOperands();

    void getObjAddr();
    void getCostAddr();

    QProcess _objdump;
    QProcessEnvironment _env;

    QString _objFile;
    QString _objdumpCmd;

    LineBuffer _line;

    instr_iterator _it;
    instr_iterator _ite;
    instr_iterator _costIt;

    Addr _objAddr;
    Addr _costAddr;
    Addr _nextCostAddr;
    Addr _dumpStartAddr;
    Addr _dumpEndAddr;

    bool _needObjAddr = true;
    bool _needCostAddr = true;
    bool _skipLineWritten = true;
    bool _isArm;

    int _noAssLines = 0;
    int _objdumpLineno = 0;

    TraceInstr* _currInstr;
};

ObjdumpParser::ObjdumpParser(TraceFunction* func)
    : _isArm{func->data()->architecture() == TraceData::ArchARM}
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::ObjdumpParser()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    auto instrMap = func->instrMap();
    assert(!instrMap->empty());

    _it = instrMap->begin();
    _ite = instrMap->end();

    _nextCostAddr = _it->addr();
    if (_isArm)
        _nextCostAddr = _nextCostAddr.alignedDown(2);

    _dumpStartAddr = _nextCostAddr;
    _dumpEndAddr = func->lastAddress() + 2;

    [[maybe_unused]] bool res = runObjdump(func);
    assert(res);
}

bool ObjdumpParser::runObjdump(TraceFunction* func)
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::runObjdump()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    TraceObject* objectFile = func->object();
    QString dir = objectFile->directory();

    if (!searchFile(dir, objectFile, func->data()))
    {
        // Should be implemented in a different manner
        qDebug() << QObject::tr("For annotated machine code, the following object file is needed");
        qDebug() << QStringLiteral("    \'%1\'").arg(objectFile->name());
        qDebug() << QObject::tr("This file cannot be found.");
        if (_isArm)
            qDebug() <<  QObject::tr("If cross-compiled, set SYSROOT variable.");

        return false;
    }
    else
    {
        objectFile->setDirectory(dir);

        _objFile = dir + '/' + objectFile->shortName();

        QString objdumpFormat = getObjDumpFormat();
        if (objdumpFormat.isEmpty())
            objdumpFormat = getObjDump();

        int margin = _isArm ? 4 : 20;

        QStringList args{"-C", "-d"};
        args << QStringLiteral("--start-address=0x%1").arg(_dumpStartAddr.toString()),
        args << QStringLiteral("--stop-address=0x%1").arg((_dumpEndAddr + margin).toString()),
        args << _objFile;

        _objdumpCmd = objdumpFormat + ' ' + args.join(' ');

        qDebug("Running \'%s\'...", qPrintable(_objdumpCmd));

        _objdump.start(objdumpFormat, args);
        if (!_objdump.waitForStarted() || !_objdump.waitForFinished())
        {
            // Should be implemented in a different manner
            qDebug() <<  QObject::tr("There is an error trying to execute the command");
            qDebug() << QStringLiteral("    \'%1\'").arg(_objdumpCmd);
            qDebug() << QObject::tr("Check that you have installed \'objdump\'.");
            qDebug() << QObject::tr("This utility can be found in the \'binutils\' package");

            return false;
        }
        else
            return true;
    }
}

bool ObjdumpParser::searchFile(QString& dir, TraceObject* o, TraceData* data)
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::searchFile()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    QString filename = o->shortName();

    if (QDir::isAbsolutePath(dir))
    {
        if (QFile::exists(dir + '/' + filename))
            return true;
        else
        {
            QString sysRoot = getSysRoot();

            if (sysRoot.isEmpty())
                return false;
            else
            {
                if (!dir.startsWith('/') && !sysRoot.endsWith('/'))
                    sysRoot += '/';
                dir.prepend(sysRoot);
                return QFile::exists(dir + '/' + filename);
            }
        }
    }
    else
    {
        QFileInfo fi(dir, filename);
        if (fi.exists()) {
            dir = fi.absolutePath();
            return true;
        }
        else
        {
            TracePart* firstPart = data->parts().first();
            if (firstPart)
            {
                QFileInfo partFile(firstPart->name());
                if (QFileInfo(partFile.absolutePath(), filename).exists())
                {
                    dir = partFile.absolutePath();
                    return true;
                }
            }

            return false;
        }
    }
}

QString ObjdumpParser::getObjDump()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::getObjDump()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    if (_env.isEmpty())
        _env = QProcessEnvironment::systemEnvironment();

    return _env.value(QStringLiteral("OBJDUMP"), QStringLiteral("objdump"));
}

QString ObjdumpParser::getObjDumpFormat()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::getObjDumpFormat()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    if (_env.isEmpty())
        _env = QProcessEnvironment::systemEnvironment();

    return _env.value(QStringLiteral("OBJDUMP_FORMAT"));
}

QString ObjdumpParser::getSysRoot()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::getSysRoot()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    if (_env.isEmpty())
        _env = QProcessEnvironment::systemEnvironment();

    return _env.value(QStringLiteral("SYSROOT"));
}

std::pair<QString, QMap<Addr, std::pair<QString, QString>>> ObjdumpParser::getInstrStrings()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::getInstrStrings()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    QMap<Addr, std::pair<QString, QString>> instrStrings;

    for (; ; _line.setPos(0))
    {
        if (_needObjAddr)
            getObjAddr();

        if (_objAddr == 0 || _objAddr > _dumpEndAddr)
            break;

        if (_needCostAddr && Addr{0} < _nextCostAddr && _nextCostAddr <= _objAddr)
            getCostAddr();

        Addr addr;
        [[maybe_unused]] QString encoding;
        QString mnemonic, operands;
        if (_nextCostAddr == 0 || _costAddr == 0 || _objAddr < _nextCostAddr)
        {
            addr = parseAddress();
            assert(addr == _objAddr);

            _line.advance(1);

            _needObjAddr = true;

            if ((_costAddr == 0 || _costAddr + 3 * GlobalConfig::context() < _objAddr) &&
                (_nextCostAddr == 0 || _objAddr < _nextCostAddr - 3 * GlobalConfig::context()))
            {
                if (_skipLineWritten || _it == _ite)
                    continue;
                else
                {
                    encoding = mnemonic = QString{};
                    operands = QStringLiteral("...");

                    _skipLineWritten = true;
                }
            }
            else
            {
                encoding = parseEncoding();
                assert(!encoding.isNull());

                mnemonic = parseMnemonic();
                operands = parseOperands();

                _skipLineWritten = false;
            }

            if (_costAddr == _objAddr)
            {
                _currInstr = std::addressof(*_costIt);
                _needCostAddr = true;
            }
            else
                _currInstr = nullptr;
        }
        else
        {
            addr = _costAddr;
            operands = QObject::tr("(No Instruction)");

            _currInstr = std::addressof(*_costIt);

            _needCostAddr = true;
            _skipLineWritten = false;
            _noAssLines++;
        }

        if (!mnemonic.isEmpty() && _currInstr)
        {
            operands.replace('<', '[');
            operands.replace('>', ']');
            instrStrings.insert(_objAddr, std::pair{mnemonic, operands});
        }
    }

    if (_noAssLines > 1)
    {
        QString message = QStringLiteral("There are %1 cost line(s) without machine code.\n"
                                         "This happens because the code of %2 does not seem "
                                         "to match the profile data file.\n"
                                         "Are you using an old profile data file or is the above"
                                         "mentioned\n"
                                         "ELF object from an updated installation/another"
                                         "machine?\n").arg(_noAssLines).arg(_objFile);

        return {message, {}};
    }
    else if (instrStrings.empty())
    {
        QString message = QStringLiteral("There seems to be an error trying to execute the command"
                                         "\'%1\'.\n"
                                         "Check that the ELF object used in the command exists.\n"
                                         "Check that you have installed \'objdump\'.\n"
                                         "This utility can be found in the \'binutils\' package.")
                                        .arg(_objdumpCmd);

        return {message, {}};
    }
    else
        return {{}, instrStrings};
}

void ObjdumpParser::getObjAddr()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::getObjAddr()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    _needObjAddr = false;
    while (true)
    {
        qint64 readBytes = _objdump.readLine(_line.absData(), _line.capacity());
        if (readBytes <= 0)
        {
            _objAddr = Addr{0};
            break;
        }
        else
        {
            _objdumpLineno++;
            if (readBytes == static_cast<qint64>(_line.capacity()))
                qDebug("ERROR: Line %d is too long\n", _objdumpLineno);
            else if (_line.absData()[readBytes - 1] == '\n')
                _line.setElem(readBytes - 1, '\0');

            _objAddr = parseAddress();
            _line.setPos(0);
            if (_dumpStartAddr <= _objAddr && _objAddr <= _dumpEndAddr)
                break;
        }
    }
}

void ObjdumpParser::getCostAddr()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::getCostAddr()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    _needCostAddr = false;
    _costIt = _it++;

    _costAddr = std::exchange(_nextCostAddr, (_it == _ite) ? Addr{0} : _it->addr());
    if (_isArm)
        _nextCostAddr = _nextCostAddr.alignedDown(2);
}

bool ObjdumpParser::isHexDigit(char c)
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::ishexDigit()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f');
}

Addr ObjdumpParser::parseAddress()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::parserAddress()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    _line.skipWhitespaces();

    Addr addr;
    int digits = addr.set(_line.relData());
    _line.advance(digits);

    return (digits == 0 || _line.elem() != ':') ? Addr{0} : addr;
}

QString ObjdumpParser::parseEncoding()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::parseEncoding()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    _line.skipWhitespaces();
    LineBuffer::pos_type start = _line.getPos();

    while (true)
    {
        if (!isHexDigit(_line.elem()) || !isHexDigit(_line.elem(1)))
            break;
        else if (_line.elem(2) == ' ')
            _line.advance(3);
        else if (!isHexDigit(_line.elem(2)) || !isHexDigit(_line.elem(3)))
            break;
        else if (_line.elem(4) == ' ')
            _line.advance(5);
        else if (!isHexDigit(_line.elem(4)) ||
                !isHexDigit(_line.elem(5)) ||
                !isHexDigit(_line.elem(6)) ||
                !isHexDigit(_line.elem(7)) ||
                _line.elem(8) != ' ')
            break;
        else
            _line.advance(9);
    }

    if (_line.getPos() > start)
        return QString::fromLatin1(_line.absData(start), _line.getPos() - start - 1);
    else
        return QString{};
}

QString ObjdumpParser::parseMnemonic()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::parseMnemonic()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    _line.skipWhitespaces();
    LineBuffer::pos_type start = _line.getPos();

    while (_line.elem() && _line.elem() != ' ' && _line.elem() != '\t')
        _line.advance(1);

    return QString::fromLatin1(_line.absData(start), _line.getPos() - start);
}

QString ObjdumpParser::parseOperands()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::parseOperands()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    _line.skipWhitespaces();

    char* operandsPos = _line.relData();
    auto operandsLen = std::min<std::size_t>(std::strlen(operandsPos),
                                             std::strchr(operandsPos, '#') - operandsPos);
    if (operandsLen > 0 && _line.elem(operandsLen - 1) == '\n')
        operandsLen--;

    if (operandsLen > 50)
        return QString::fromLatin1(operandsPos, 47) + QStringLiteral("...");
    else
        return QString::fromLatin1(operandsPos, operandsLen);
}

} // unnamed namespace

bool CFGExporter::fillInstrStrings(TraceFunction* func)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::fillInstrStrings()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    assert(func);

    if (_nodeMap.empty())
        return false;

    ObjdumpParser parser{func};
    auto [message, instrStrings] = parser.getInstrStrings();
    if (instrStrings.empty())
    {
        _errorMessage = message;
        return false;
    }

    for (auto it = _nodeMap.begin(), ite = _nodeMap.end(); it != ite; ++it)
    {
        auto [firstAddr, lastAddr] = it.key();
        CFGNode& node = it.value();

        auto lastIt = instrStrings.find(lastAddr);
        assert(lastIt != instrStrings.end());

        node.insertInstructions(instrStrings.find(firstAddr), std::next(lastIt));
    }

    return true;
}

void CFGExporter::dumpNodes(QTextStream& ts)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::dumpNodes()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    for (auto& node : _nodeMap)
    {
        if (detailsLevel(node.basicBlock()) == DetailsLevel::pcOnly)
            dumpNodeReduced(ts, node);
        else
            dumpNodeExtended(ts, node);
    }
}

void CFGExporter::dumpNodeReduced(QTextStream& ts, const CFGNode& node)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::dumpNodeReduced()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    const TraceBasicBlock* bb = node.basicBlock();
    QString firstAddr = bb->firstAddr().toString();

    ts << QStringLiteral("  b%1b%2 [shape=record, label=\"")
                        .arg(firstAddr)
                        .arg(bb->lastAddr().toString());

    if (_layout == Layout::TopDown)
        ts << '{';

    ts << QStringLiteral(" cost: %1 | %2 ")
                        .arg(QString::number(node.self))
                        .arg("0x" + firstAddr);

    if (_layout == Layout::TopDown)
        ts << '}';

    ts << "\"]\n";
}

void CFGExporter::dumpNodeExtended(QTextStream& ts, const CFGNode& node)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::dumpNodeExtended()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    const TraceBasicBlock* bb = node.basicBlock();
    assert(bb);
    QString firstAddr = bb->firstAddr().toString();
    QString lastAddr = bb->lastAddr().toString();

    ts << QStringLiteral("  b%1b%2 [shape=plaintext, label=<\n"
                         "  <table border=\"0\" cellborder=\"1\" cellspacing=\"0\">\n"
                         "  <tr>\n"
                         "    <td colspan=\"2\">cost: %3</td>\n"
                         "  </tr>\n"
                         "  <tr>\n"
                         "    <td colspan=\"2\">%4</td>\n"
                         "  </tr>\n").arg(firstAddr).arg(lastAddr)
                                     .arg(QString::number(node.self))
                                     .arg("0x" + firstAddr);

    auto strIt = node.begin();
    auto instrIt = bb->begin();
    auto lastInstrIt = std::prev(bb->end());

    if (instrIt != lastInstrIt)
    {
        ts << QStringLiteral("  <tr>\n"
                             "    <td port=\"IL%1\">%2</td>\n"
                             "    <td>%3</td>\n"
                             "  </tr>\n").arg((*instrIt)->addr().toString())
                                         .arg(strIt->first).arg(strIt->second);

        for (++instrIt; instrIt != lastInstrIt; ++instrIt, ++strIt)
        {
            ts << QStringLiteral("  <tr>\n"
                                 "    <td>%1</td>\n"
                                 "    <td>%2</td>\n"
                                 "  </tr>\n").arg(strIt->first).arg(strIt->second);
        }
    }

    ts << QStringLiteral("  <tr>\n"
                         "    <td port=\"IL%1\">%2</td>\n"
                         "    <td port=\"IR%3\">%4</td>\n"
                         "  </tr>\n"
                         "  </table>>]\n").arg(lastAddr).arg(strIt->first)
                                         .arg(lastAddr).arg(strIt->second);
}

namespace
{

void dumpNonFalseBranchColor(QTextStream& ts, const TraceBranch* br)
{
    const char* color;
    switch (br->brType())
    {
        case TraceBranch::Type::true_:
            color = "blue";
            break;
        case TraceBranch::Type::unconditional:
            color = "black";
            break;
        case TraceBranch::Type::indirect:
            color = "green";
            break;
        default:
            assert(false);
    }

    ts << QStringLiteral("color=%1, ").arg(color);
}

void dumpCyclicEdge(QTextStream& ts, const TraceBranch* br, bool isReduced)
{
    assert(br->bbFrom() == br->bbTo());

    const TraceBasicBlock* bb = br->bbFrom();

    QString firstAddr = bb->firstAddr().toString();
    QString lastAddr = bb->lastAddr().toString();

    if (isReduced)
    {
        ts << QStringLiteral("  b%1b%2:w -> b%4b%5:w [constraint=false, ")
                            .arg(firstAddr).arg(lastAddr).arg(firstAddr).arg(lastAddr);
    }
    else
    {
        ts << QStringLiteral("  b%1b%2:IL%3:w -> b%4b%5:IL%6:w [constraint=false, ")
                            .arg(firstAddr).arg(lastAddr).arg(lastAddr)
                            .arg(firstAddr).arg(lastAddr).arg(firstAddr);
    }

    dumpNonFalseBranchColor(ts, br);
}

void dumpRegularBranch(QTextStream& ts, const TraceBranch* br)
{
    const TraceBasicBlock* bbFrom = br->bbFrom();
    const TraceBasicBlock* bbTo = br->bbTo();

    QString firstAddrFrom = bbFrom->firstAddr().toString();
    QString lastAddrFrom = bbFrom->lastAddr().toString();
    QString firstAddrTo = bbTo->firstAddr().toString();
    QString lastAddrTo = bbTo->lastAddr().toString();

    ts << QStringLiteral("  b%1b%2:s -> b%3b%4:n [")
                        .arg(firstAddrFrom).arg(lastAddrFrom).arg(firstAddrTo).arg(lastAddrTo);

    if (br->brType() == TraceBranch::Type::false_)
        ts << "color=red, ";
    else
        dumpNonFalseBranchColor(ts, br);
}

} // unnamed namespace

void CFGExporter::dumpEdges(QTextStream& ts, DumpType type)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::dumpEdges()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    for (auto& edge : _edgeMap)
    {
        TraceBranch* br = edge.branch();
        assert(br);

        if (br->isCycle())
        {
            bool isReduced = (detailsLevel(br->bbFrom()) == DetailsLevel::pcOnly);
            dumpCyclicEdge(ts, br, isReduced);
        }
        else
            dumpRegularBranch(ts, br);

        if (type == DumpType::internal)
        {
            ts << QStringLiteral("label=\"%1 %2\"]\n")
                            .arg(br->instrFrom()->addr().toString())
                            .arg(br->instrTo()->addr().toString());
        }
        else
            ts << QStringLiteral("label=\"%1\"]\n").arg(br->executedCount().pretty());
    }
}

CFGNode* CFGExporter::toCFGNode(QString s)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::toBasicBlock()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (s[0] == 'b')
    {
        qsizetype i = s.indexOf('b', 1);
        if (i != -1)
        {
            bool ok;
            qulonglong from = s.mid(1, i - 1).toULongLong(&ok, 16);
            if (ok)
            {
                qulonglong to = s.mid(i + 1).toULongLong(&ok, 16);
                if (ok)
                {
                    auto it = _nodeMap.find(std::make_pair(Addr{from}, Addr{to}));
                    return (it == _nodeMap.end()) ? nullptr : std::addressof(*it);
                }
            }
        }
    }

    return nullptr;
}

bool CFGExporter::savePrompt(QWidget* parent, TraceFunction* func,
                             EventType* eventType, ProfileContext::Type groupType,
                             Layout layout, const details_map_type& map)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::savePrompt()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    static constexpr const char* filter1 = "text/vnd.graphviz";
    static constexpr const char* filter2 = "application/pdf";
    static constexpr const char* filter3 = "application/postscript";

    QFileDialog saveDialog{parent, QObject::tr("Export Graph")};
    saveDialog.setMimeTypeFilters(QStringList{filter1, filter2, filter3});
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);

    if (saveDialog.exec())
    {
        QString intendedName = saveDialog.selectedFiles().first();
        if (intendedName.isNull() || intendedName.isEmpty())
            return false;

        QTemporaryFile maybeTemp;
        maybeTemp.open();

        QString dotName;
        QString dotRenderType;

        QString mime = saveDialog.selectedMimeTypeFilter();
        if (mime == filter1)
        {
            dotName = intendedName;
            dotRenderType = "";
        }
        else if (mime == filter2)
        {
            dotName = maybeTemp.fileName();
            dotRenderType = "-Tpdf";
        }
        else // mime == filter3
        {
            dotName = maybeTemp.fileName();
            dotRenderType = "-Tps";
        }

        CFGExporter ge{func, eventType, groupType, dotName};
        ge.setLayout(layout);
        ge.setDetailsMap(map);

        bool wrote = ge.writeDot(DumpType::external);
        if (wrote && mime != filter1)
        {
            QProcess proc;
            proc.setStandardOutputFile(intendedName, QFile::Truncate);
            proc.start("dot", QStringList{dotRenderType, dotName}, QProcess::ReadWrite);
            proc.waitForFinished();

            if (proc.exitStatus() == QProcess::NormalExit)
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(intendedName));
                return true;
            }
            else
                return false;
        }
        else
            return wrote;
    }
    else
        return false;
}

// ======================================================================================

//
// CanvasCFGNode
//

CanvasCFGNode::CanvasCFGNode(ControlFlowGraphView* view, CFGNode* node,
                             qreal x, qreal y, qreal w, qreal h) :
    QGraphicsRectItem{x, y, w, h}, _node{node}, _view{view}
{
    #ifdef CANVASCFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGNode::CanvasCFGNode()" << "\033[0m";
    #endif // CANVASCFGNODE_DEBUG

    if (!_node || !_view)
        return;

    setBackColor(Qt::white);
    update();

    SubCost total = node->basicBlock()->function()->subCost(view->eventType());
    double selfPercentage = 100.0 * _node->self / total;

    setPosition(0, DrawParams::TopCenter);

    // set inclusive cost
    if (GlobalConfig::showPercentage())
        setText(0, QStringLiteral("%1 %")
                                 .arg(selfPercentage, 0, 'f', GlobalConfig::percentPrecision()));
    else
        setText(0, SubCost(_node->self).pretty());

    // set percentage bar
    setPixmap(0, percentagePixmap(25, 10, static_cast<int>(selfPercentage + 0.5), Qt::blue, true));

    // set tool tip (balloon help) with the name of a basic block and percentage
    setToolTip(QStringLiteral("%1").arg(text(0)));
}

void CanvasCFGNode::setSelected(bool s)
{
    #ifdef CANVASCFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGNode::setSelected()" << "\033[0m";
    #endif // CANVASCFGNODE_DEBUG

    StoredDrawParams::setSelected(s);
    update();
}

void CanvasCFGNode::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    #ifdef CANVASCFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGNode::paint()" << "\033[0m";
    #endif // CANVASCFGNODE_DEBUG

    bool reduced = _view->isReduced(_node);

    QRectF rectangle = rect();
    qreal topLineY = rectangle.y();

    qreal step = rectangle.height() / (reduced ? 2 : _node->instrNumber() + 2);

    p->fillRect(rectangle.x() + 1, topLineY + 1, rectangle.width(), step * 2, Qt::gray);
    topLineY += step;

    p->drawText(rectangle.x(), topLineY, rectangle.width(), step,
                Qt::AlignCenter, "0x" + _node->basicBlock()->firstAddr().toString());
    p->drawLine(rectangle.x(), topLineY,
                rectangle.x() + rectangle.width(), topLineY);

    if (!reduced)
    {
        topLineY += step;

        QFontMetrics fm = _view->fontMetrics();
        auto mnemonicComp = [&fm](auto& pair1, auto& pair2)
        {
            return fm.size(Qt::TextSingleLine, pair1.first).width() <
                   fm.size(Qt::TextSingleLine, pair2.first).width();
        };

        auto maxLenIt = std::max_element(_node->begin(), _node->end(), mnemonicComp);

        int shift = fm.size(Qt::TextSingleLine, maxLenIt->first).width() + 4;
        for (auto &[mnemonic, args] : *_node)
        {
            p->drawText(rectangle.x() + 2, topLineY, shift, step,
                        Qt::AlignLeft, mnemonic);
            p->drawText(rectangle.x() + shift + 2, topLineY, rectangle.width() - shift, step,
                        Qt::AlignLeft, args);
            p->drawLine(rectangle.x(), topLineY,
                        rectangle.x() + rectangle.width(), topLineY);

            topLineY += step;
        }

        p->drawLine(rectangle.x() + shift, rectangle.y() + step * 2,
                    rectangle.x() + shift, rectangle.y() + rectangle.height());
    }

    if (StoredDrawParams::selected())
    {
        QPen pen{Qt::darkGreen};
        pen.setWidth(2);
        p->setPen(pen);

        p->drawRect(rectangle);
    }
    else
        p->drawRect(rectangle);

    RectDrawing d{rectangle.toRect()};
    d.drawField(p, 0, this);
}

// ======================================================================================

//
// CanvasCFGEdgeLabel
//

CanvasCFGEdgeLabel::CanvasCFGEdgeLabel(ControlFlowGraphView* v, CanvasCFGEdge* ce,
                                       qreal x, qreal y, qreal w, qreal h) :
    QGraphicsRectItem{x, y, w, h}, _ce{ce}, _view{v}
{
    #ifdef CANVASCFGEDGELABEL_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdgeLabel::CanvasCFGEdgeLabel()" << "\033[0m";
    #endif // CANVASCFGEDGELABEL_DEBUG

    CFGEdge* e = _ce->edge();
    if (!e)
        return;

    setPosition(0, DrawParams::TopCenter);

    #if 0
    auto total = calculateTotalInclusiveCost(_view);
    auto inclPercentage = 100.0 * e->cost / total;

    _percentage = std::min(inclPercentage, 100.0);

    if (GlobalConfig::showPercentage())
        setText(1, QStringLiteral("%1 %")
                                 .arg(inclPercentage, 0, 'f', GlobalConfig::percentPrecision()));
    else
        setText(1, SubCost(e->cost).pretty());
    #endif

    SubCost count = e->branch()->executedCount();
    setText(0, QStringLiteral("%1 x").arg(count.pretty()));

    #if 0
    int pixPos;
    if (static_cast<TraceItemView*>(_view)->data()->maxCallCount() > 0)
    {
        setPosition(0, DrawParams::TopCenter);

        SubCost count{std::max(e->count, 1.0)};
        setText(0, QStringLiteral("%1 x").arg(count.pretty()));
        setToolTip(QStringLiteral("%1 (%2)").arg(text(0)).arg(text(1)));

        pixPos = 0;
    }
    else
        pixPos = 1;
    #endif

    if (e->bbTo() && e->bbFrom() == e->bbTo())
    {
        QFontMetrics fm{font()};
        QPixmap pixmap = QIcon::fromTheme(QStringLiteral("edit-undo")).pixmap(fm.height());
        setPixmap(0, pixmap);
    }
    else
        setPixmap(0, percentagePixmap(25, 10, count, Qt::blue, true));
}

void CanvasCFGEdgeLabel::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    #ifdef CANVASCFGEDGELABEL_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdgeLabel::paint()" << "\033[0m";
    #endif // CANVASCFGEDGELABEL_DEBUG
#if 0
#if QT_VERSION >= 0x040600
    if (option->levelOfDetailFromTransform(p->transform()) < 0.5)
        return;
#else
    if (option->levelOfDetail < 0.5)
        return;
#endif
#endif

    RectDrawing drawer{rect().toRect()};

    drawer.drawField(p, 0, this);
    #if 0
    drawer.drawField(p, 1, this);
    #endif
}

// ======================================================================================

//
// CanvasCFGEdgeArrow
//

CanvasCFGEdgeArrow::CanvasCFGEdgeArrow(CanvasCFGEdge* ce) : _ce{ce}
{
    #ifdef CANVASCFGEDGEARROW_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdgeArrow::CanvasCFGEdgeArrow()" << "\033[0m";
    #endif // CANVASCFGEDGEARROW_DEBUG
}

void CanvasCFGEdgeArrow::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    #ifdef CANVASCFGEDGEARROW_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdgeArrow::paint()" << "\033[0m";
    #endif // CANVASCFGEDGEARROW_DEBUG

    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(Qt::black);
    p->drawPolygon(polygon(), Qt::OddEvenFill);
}

// ======================================================================================

//
// CanvasCFGEdge
//

CanvasCFGEdge::CanvasCFGEdge(CFGEdge* e) : _edge{e}
{
    #ifdef CANVASCFGEDGE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdge::CanvasCFGEdge()" << "\033[0m";
    #endif // CANVASCFGEDGE_DEBUG

    setFlag(QGraphicsItem::ItemIsSelectable);
}

void CanvasCFGEdge::setLabel(CanvasCFGEdgeLabel* l)
{
    #ifdef CANVASCFGEDGE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdge::setLabel()" << "\033[0m";
    #endif // CANVASCFGEDGE_DEBUG

    _label = l;

    if (_label)
    {
        QString tip = QStringLiteral("%1 (%2)").arg(l->text(0)).arg(l->text(1));

        setToolTip(tip);
        if (_arrow)
            _arrow->setToolTip(tip);

        _thickness = 0.9;
    }
}

void CanvasCFGEdge::setArrow(CanvasCFGEdgeArrow* a)
{
    #ifdef CANVASCFGEDGE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdge::setArrow()" << "\033[0m";
    #endif // CANVASCFGEDGE_DEBUG

    _arrow = a;

    if (_arrow && _label)
        a->setToolTip(QStringLiteral("%1 (%2)")
                                    .arg(_label->text(0)).arg(_label->text(1)));
}

void CanvasCFGEdge::setControlPoints(const QPolygon& p)
{
    #ifdef CANVASCFGEDGE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdge:setControlPoints()" << "\033[0m";
    #endif // CANVASCFGEDGE_DEBUG

    _points = p;

    QPainterPath path;
    path.moveTo(p[0]);
    for (decltype(p.size()) i = 1; i < p.size(); i += 3)
        path.cubicTo(p[i], p[(i + 1) % p.size()], p[(i + 2) % p.size()]);

    setPath(path);
}

void CanvasCFGEdge::setSelected(bool s)
{
    #ifdef CANVASCFGEDGE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdge:setSelected()" << "\033[0m";
    #endif // CANVASCFGEDGE_DEBUG

    QGraphicsItem::setSelected(s);
    update();
}

void CanvasCFGEdge::paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget*)
{
    #ifdef CANVASCFGEDGE_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdge:paint()" << "\033[0m";
    #endif // CANVASCFGEDGE_DEBUG

    p->setRenderHint(QPainter::Antialiasing);

#if QT_VERSION >= 0x040600
    qreal levelOfDetail = option->levelOfDetailFromTransform(p->transform());
#else
    qreal levelOfDetail = option->levelOfDetail;
#endif

    QPen mypen = pen();

    mypen.setWidthF(isSelected() ? 2.0 : 1.0 / levelOfDetail * _thickness);
    p->setPen(mypen);
    p->drawPath(path());
}

// ======================================================================================

//
// ControlFlowGraphView
//

ControlFlowGraphView::ControlFlowGraphView(TraceItemView* parentView, QWidget* parent,
                                           const QString& name) :
    QGraphicsView(parent), TraceItemView(parentView)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::ControlFlowGraphView" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    setObjectName(name);
    setWhatsThis(whatsThis());
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NoSystemBackground, true);

    _panningView = new PanningView(this);

    _panningView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _panningView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _panningView->raise();
    _panningView->hide();

    connect(_panningView, &PanningView::zoomRectMoved,
            this, &ControlFlowGraphView::zoomRectMoved);
    connect(_panningView, &PanningView::zoomRectMoveFinished,
            this, &ControlFlowGraphView::zoomRectMoveFinished);
    connect(std::addressof(_renderTimer), &QTimer::timeout,
            this, &ControlFlowGraphView::showRenderWarning);
}

ControlFlowGraphView::~ControlFlowGraphView()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::~ControlFlowGraphView" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_scene)
    {
        _panningView->setScene(nullptr);
        setScene(nullptr);
        delete _scene;
    }

    delete _panningView;
}

QString ControlFlowGraphView::whatsThis() const
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::whatsThis" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    return QObject::tr("This is Control Flow Graph by dWX1268804");
}

bool ControlFlowGraphView::isReduced(CFGNode* node) const
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::isReduced" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    return _exporter.detailsLevel(node->basicBlock()) == CFGExporter::DetailsLevel::pcOnly;
}

TraceFunction* ControlFlowGraphView::getFunction()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::getFunction" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    assert(_activeItem);

    switch (_activeItem->type())
    {
        case ProfileContext::BasicBlock:
            return static_cast<TraceBasicBlock*>(_activeItem)->function();
        case ProfileContext::Branch:
            return static_cast<TraceBranch*>(_activeItem)->bbFrom()->function();
        case ProfileContext::Call:
            return static_cast<TraceCall*>(_activeItem)->caller();
        case ProfileContext::Function:
            return static_cast<TraceFunction*>(_activeItem);
        default:
            assert(false);
            return nullptr;
    }
}

void ControlFlowGraphView::zoomRectMoved(qreal dx, qreal dy)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::zoomRectMoved" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    //FIXME if (leftMargin()>0) dx = 0;
    //FIXME if (topMargin()>0) dy = 0;

    QScrollBar* hBar = horizontalScrollBar();
    QScrollBar* vBar = verticalScrollBar();
    hBar->setValue(hBar->value() + static_cast<int>(dx));
    vBar->setValue(vBar->value() + static_cast<int>(dy));
}

void ControlFlowGraphView::zoomRectMoveFinished()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::zoomRectMoveFinished" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_zoomPosition == ZoomPosition::Auto)
        updateSizes();
}

void ControlFlowGraphView::showRenderWarning()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::showRenderWarning" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString s;
    if (_renderProcess)
        s = QObject::tr("Warning: a long lasting graph layouting is in progress.\n"
                        "Reduce node/edge limits for speedup.\n");
    else
        s = QObject::tr("Layouting stopped.\n");

    s.append(QObject::tr("The call graph has %1 nodes and %2 edges.\n")
                        .arg(_exporter.nodeCount()).arg(_exporter.edgeCount()));

    showText(s);
}

void ControlFlowGraphView::showRenderError(const QString& text)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::showRenderError" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString err = QObject::tr("No graph available because the layouting process failed.\n");
    if (_renderProcess)
        err += QObject::tr("Trying to run the following command did not work:\n"
                           "'%1'\n").arg(_renderProcessCmdLine);
    err += QObject::tr("Please check that 'dot' is installed (package GraphViz).");

    if (!text.isEmpty())
        err += QStringLiteral("\n\n%1").arg(text);

    showText(err);
}

void ControlFlowGraphView::stopRendering()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug("ControlFlowGraphView::stopRendering: Killing QProcess %p", _renderProcess);
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (!_renderProcess)
        return;

    _renderProcess->kill();
    _renderProcess->deleteLater();
    _renderProcess = nullptr;

    _unparsedOutput.clear();

    _renderTimer.setSingleShot(true);
    _renderTimer.start(200);
}

void ControlFlowGraphView::readDotOutput()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::readDotOutput" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    auto process = qobject_cast<QProcess*>(sender());
    qDebug("ControlFlowGraphView::readDotOutput: QProcess %p", process);

    if (_renderProcess && process == _renderProcess)
        _unparsedOutput.append(QString::fromLocal8Bit(_renderProcess->readAllStandardOutput()));
    else
        process->deleteLater();
}

void ControlFlowGraphView::dotError()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::dotError" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    auto process = qobject_cast<QProcess*>(sender());
    qDebug("ControlFlowGraphView::dotError: Got %d from QProcess %p", process->error(), process);

    if (_renderProcess && process == _renderProcess)
    {
        showRenderError(QString::fromLocal8Bit(_renderProcess->readAllStandardError()));

        _renderProcess->deleteLater();
        _renderProcess = nullptr;
    }
    else
        process->deleteLater();
}

void ControlFlowGraphView::dotExited()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::dotExited" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    auto process = qobject_cast<QProcess*>(sender());
    qDebug("ControlFlowGraphView::dotExited: QProcess %p", process);

    if (!_renderProcess || process != _renderProcess)
    {
        process->deleteLater();
        return;
    }

    _unparsedOutput.append(QString::fromLocal8Bit(_renderProcess->readAllStandardOutput()));
    _renderProcess->deleteLater();
    _renderProcess = nullptr;

    _renderTimer.stop();
    viewport()->setUpdatesEnabled(false);
    clear();

    parseDot();
    checkScene();

    _exporter.sortEdges();

    _panningZoom = 0.0;
    _panningView->setScene(_scene);
    setScene(_scene);

    centerOnSelectedNodeOrEdge();

    updateSizes();

    _scene->update();
    viewport()->setUpdatesEnabled(true);

    #if 0 // I've got the strongest feeling this piece of code is useless
    delete _renderProcess;
    _renderProcess = nullptr;
    #endif
}

void ControlFlowGraphView::parseDot()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::parseDot" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QTextStream dotStream{std::addressof(_unparsedOutput), QIODevice::ReadOnly};
    _scaleY = 8 + 3 * fontMetrics().height();

    QString cmd;
    for (auto lineno = 1; ; lineno++)
    {
        QString line = dotStream.readLine();

        if (line.isNull())
            break;
        else if (!line.isEmpty())
        {
            QTextStream lineStream{std::addressof(line), QIODevice::ReadOnly};
            lineStream >> cmd;

            if (cmd == QLatin1String("stop"))
                break;
            else if (cmd == QLatin1String("graph"))
                setupScreen(lineStream, lineno);
            else if (!_scene)
                qDebug() << "Ignoring \'" << cmd << "\' without \'graph\' form dot ("
                            << _exporter.filename() << ":" << lineno << ")";
            else if (cmd == QLatin1String("node"))
                parseNode(lineStream);
            else if (cmd == QLatin1String("edge"))
                parseEdge(lineStream, lineno);
        }
    }
}

void ControlFlowGraphView::setupScreen(QTextStream& lineStream, int lineno)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::setupScreen" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString dotWidthString, dotHeightString;
    lineStream >> _dotHeight >> dotWidthString >> dotHeightString;

    _dotHeight = dotHeightString.toDouble(); // overrides previous unused value

    if (_scene)
        qDebug() << "Ignoring 2nd \'graph\' from dot ("
                 << _exporter.filename() << ":" << lineno << ")";
    else
    {
        QSize pScreenSize = QApplication::primaryScreen()->size();

        _xMargin = 50;
        auto w = static_cast<int>(_scaleX * dotWidthString.toDouble());
        if (w < pScreenSize.width())
            _xMargin += (pScreenSize.width() - w) / 2;

        _yMargin = 50;
        auto h = static_cast<int>(_scaleY * _dotHeight);
        if (h < pScreenSize.height())
            _yMargin += (pScreenSize.height() - h) / 2;

        _scene = new QGraphicsScene{0.0, 0.0, static_cast<qreal>(w + 2 * _xMargin),
                                              static_cast<qreal>(h + 2 * _yMargin)};
        _scene->setBackgroundBrush(Qt::white);
    }
}

std::pair<int, int> ControlFlowGraphView::calculateSizes(QTextStream& lineStream)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::calculateSizes" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString xStr, yStr;
    lineStream >> xStr >> yStr;

    auto xx = static_cast<int>(_scaleX * xStr.toDouble() + _xMargin);
    auto yy = static_cast<int>(_scaleY * (_dotHeight - yStr.toDouble()) + _yMargin);

    return std::pair{xx, yy};
}

void ControlFlowGraphView::parseNode(QTextStream& lineStream)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::parseNode" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString nodeName;
    lineStream >> nodeName;

    auto [xx, yy] = calculateSizes(lineStream);

    QString nodeWidth, nodeHeight;
    lineStream >> nodeWidth >> nodeHeight;

    CFGNode* node = _exporter.toCFGNode(nodeName);
    if (node)
    {
        assert(node->instrNumber() > 0);
        node->setVisible(true);
        qreal w = (_scaleX - 4.5) * nodeWidth.toDouble();
        qreal h = _scaleY * nodeHeight.toDouble();

        auto rItem = new CanvasCFGNode{this, node, xx - w / 2, yy - h / 2, w, h};
        rItem->setZValue(1.0);
        rItem->show();

        node->setCanvasNode(rItem);

        _scene->addItem(rItem);

        if (node->basicBlock() == selectedItem())
        {
            _selectedNode = node;
            rItem->setSelected(true);
        }
        else
            rItem->setSelected(node == _selectedNode);
    }
    else
        qDebug("Warning: Unknown basic block \'%s\' ?!", qPrintable(nodeName));
}

namespace
{

QColor getArrowColor(CFGEdge* edge)
{
    assert(edge);
    assert(edge->branch());

    QColor arrowColor;
    switch(edge->branch()->brType())
    {
        case TraceBranch::Type::unconditional:
            arrowColor = Qt::black;
            break;
        case TraceBranch::Type::true_:
            arrowColor = Qt::blue;
            break;
        case TraceBranch::Type::false_:
            arrowColor = Qt::red;
            break;
        case TraceBranch::Type::indirect:
            arrowColor = Qt::darkGreen;
            break;
        default:
            assert(false);
    }

    return arrowColor;
}

CanvasCFGEdge* createEdge(CFGEdge* edge, const QPolygon& poly, QColor arrowColor)
{
    auto sItem = new CanvasCFGEdge{edge};
    sItem->setControlPoints(poly);
    sItem->setPen(QPen{arrowColor});
    sItem->setZValue(0.5);
    sItem->show();

    edge->setCanvasEdge(sItem);

    return sItem;
}

CanvasCFGEdgeArrow* createArrow(CanvasCFGEdge* sItem, const QPolygon& poly, QColor arrowColor)
{
    QPoint headPoint{poly.point(poly.size() - 1)};
    QPoint arrowDir{headPoint - poly.point(poly.size() - 2)};
    assert(!arrowDir.isNull());

    auto length = static_cast<qreal>(arrowDir.x() * arrowDir.x() +
                                     arrowDir.y() * arrowDir.y());
    arrowDir *= 10.0 / std::sqrt(length);

    QPolygon arrow;
    arrow << QPoint{headPoint + arrowDir};
    arrow << QPoint{headPoint + QPoint{arrowDir.y() / 2, -arrowDir.x() / 2}};
    arrow << QPoint{headPoint + QPoint{-arrowDir.y() / 2, arrowDir.x() / 2}};

    auto aItem = new CanvasCFGEdgeArrow{sItem};
    aItem->setPolygon(arrow);
    aItem->setBrush(arrowColor);
    aItem->setZValue(1.5);
    aItem->show();

    sItem->setArrow(aItem);

    return aItem;
}

} // unnamed namespace

void ControlFlowGraphView::parseEdge(QTextStream& lineStream, int lineno)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::parseEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString node;
    lineStream >> node >> node; // ignored

    QPolygon poly = getEdgePolygon(lineStream, lineno);
    if (poly.empty())
        return;

    CFGEdge* edge = getEdgeFromDot(lineStream, lineno);
    if (!edge)
        return;

    edge->setVisible(true);

    QColor arrowColor = getArrowColor(edge);

    CanvasCFGEdge* sItem = createEdge(edge, poly, arrowColor);

    _scene->addItem(sItem);
    _scene->addItem(createArrow(sItem, poly, arrowColor));

    if (edge->branch() == selectedItem())
    {
        _selectedEdge = edge;
        sItem->setSelected(true);
    }
    else
        sItem->setSelected(edge == _selectedEdge);

    auto [xx, yy] = calculateSizes(lineStream);
    auto lItem = new CanvasCFGEdgeLabel{this, sItem,
                                        static_cast<qreal>(xx - 60),
                                        static_cast<qreal>(yy - 10),
                                        100.0, 20.0};
    _scene->addItem(lItem);
    lItem->setZValue(1.5);
    sItem->setLabel(lItem);

    lItem->show();
}

CFGEdge* ControlFlowGraphView::getEdgeFromDot(QTextStream& lineStream, int lineno)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::getEdgeFromDotReduced" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString addrFrom;
    lineStream >> addrFrom;
    addrFrom.remove(0, 1);

    bool ok;
    qulonglong from = addrFrom.toULongLong(&ok, 16);
    assert(ok);
    Addr fromAddr{from};

    QString addrTo;
    lineStream >> addrTo;
    addrTo.remove(addrTo.length() - 1, 1);

    qulonglong to = addrTo.toULongLong(&ok, 16);
    assert(ok);
    Addr toAddr{to};

    CFGEdge* edge = _exporter.findEdge(fromAddr, toAddr);
    if (!edge)
    {
        qDebug() << "Unknown edge \'" << addrFrom << "\'-\'" << addrTo << "\' from dot ("
                << _exporter.filename() << ":" << lineno << ")";
    }

    return edge;
}


QPolygon ControlFlowGraphView::getEdgePolygon(QTextStream& lineStream, int lineno)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::getEdgePolygon" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    int nPoints;
    lineStream >> nPoints;
    assert(nPoints > 1);

    QPolygon poly{nPoints};

    for (auto i = 0; i != nPoints; ++i)
    {
        if (lineStream.atEnd())
        {
            qDebug("ControlFlowGraphView: Can not read %d spline nPoints (%s:%d)",
                    nPoints, qPrintable(_exporter.filename()), lineno);
            return QPolygon{};
        }

        auto [xx, yy] = calculateSizes(lineStream);
        poly.setPoint(i, xx, yy);
    }

    return poly;
}

void ControlFlowGraphView::checkScene()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::checkSceneAndActiveItems" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (!_scene)
    {
        _scene = new QGraphicsScene;

        if (_exporter.CFGAvailable())
        {
            QString message = QObject::tr("Error running the graph layouting tool.\n"
                                          "Please check that \'dot\' is installed"
                                          "(package Graphviz).");
            _scene->addSimpleText(message);
        }
        else
            _scene->addSimpleText(_exporter.errorMessage());

        centerOn(0, 0);
    }
}

void ControlFlowGraphView::centerOnSelectedNodeOrEdge()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::centerOnSelectedNodeOrEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    CanvasCFGNode* sNode = nullptr;
    if (_selectedNode)
        sNode = _selectedNode->canvasNode();
    else if (_selectedEdge)
    {
        if (_selectedEdge->nodeFrom())
            sNode = _selectedEdge->nodeFrom()->canvasNode();

        if (!sNode && _selectedEdge->nodeTo())
            sNode = _selectedEdge->nodeTo()->canvasNode();
    }

    if (sNode)
    {
        if (_prevSelectedNode)
        {
            if (rect().contains(_prevSelectedPos))
            {
                QPointF wCenter = mapToScene(viewport()->rect().center());
                QPointF prevPos = mapToScene(_prevSelectedPos);
                centerOn(sNode->rect().center() + wCenter - prevPos);
            }
            else
                ensureVisible(sNode);
        }
        else
            centerOn(sNode);
    }
}

void ControlFlowGraphView::zoomPosTriggered(QAction* a)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::zoomPosTriggered" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    _zoomPosition = static_cast<ZoomPosition>(a->data().toInt());
    updateSizes();
}

void ControlFlowGraphView::layoutTriggered(QAction* a)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::layoutTriggered" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    _exporter.setLayout(static_cast<CFGExporter::Layout>(a->data().toInt()));
    refresh(false);
}

void ControlFlowGraphView::minimizationTriggered(QAction* a)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::minimizationTriggered" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    TraceFunction* func = getFunction();
    uint64 totalCost = func->subCost(_eventType).v;
    double percentage = a->data().toDouble();

    _exporter.setMinimalCostPercentage(percentage);
    _exporter.minimizeBBsWithCostLessThan(percentage * totalCost / 100);
    refresh(false);
}

void ControlFlowGraphView::resizeEvent(QResizeEvent* e)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::resizeEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QGraphicsView::resizeEvent(e);
    if (_scene)
        updateSizes(e->size());
}

void ControlFlowGraphView::mouseEvent(void (TraceItemView::* func)(CostItem*), QGraphicsItem* item)
{
    assert(item);

    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::mouseEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (item->type() == CanvasParts::Node)
    {
        CFGNode* node = static_cast<CanvasCFGNode*>(item)->node();
        (this->*func)(node->basicBlock());
    }
    else
    {
        if (item->type() == CanvasParts::EdgeLabel)
            item = static_cast<CanvasCFGEdgeLabel*>(item)->canvasEdge();
        else if (item->type() == CanvasParts::EdgeArrow)
            item = static_cast<CanvasCFGEdgeArrow*>(item)->canvasEdge();

        if (item->type() == CanvasParts::Edge)
        {
            CFGEdge* edge = static_cast<CanvasCFGEdge*>(item)->edge();

            if (edge->branch())
                (this->*func)(edge->branch());
        }
    }
}

void ControlFlowGraphView::mousePressEvent(QMouseEvent* event)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::mousePressEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    setFocus();

    if (event->button() == Qt::LeftButton)
        _isMoving = true;

    QGraphicsItem* item = itemAt(event->pos());
    if (item)
        mouseEvent(&TraceItemView::selected, item);

    _lastPos = event->pos();
}

void ControlFlowGraphView::mouseDoubleClickEvent(QMouseEvent* event)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::mouseDoubleClickEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QGraphicsItem* item = itemAt(event->pos());
    if (!item)
        return;

    mouseEvent(&TraceItemView::activated, item);

    if (_selectedNode)
    {
        _exporter.switchDetailsLevel(_selectedNode->basicBlock());
        _exporter.setMinimalCostPercentage(-1);
        refresh(false);
    }

    centerOnSelectedNodeOrEdge();
}

void ControlFlowGraphView::mouseMoveEvent(QMouseEvent* event)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::mouseMoveEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_isMoving)
    {
        QPoint delta = event->pos() - _lastPos;
        QScrollBar* hBar = horizontalScrollBar();
        QScrollBar* vBar = verticalScrollBar();

        hBar->setValue(hBar->value() - delta.x());
        vBar->setValue(vBar->value() - delta.y());

        _lastPos = event->pos();
    }
}

void ControlFlowGraphView::mouseReleaseEvent(QMouseEvent*)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::mouseReleaseEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    _isMoving = false;
    if (_zoomPosition == ZoomPosition::Auto)
        updateSizes();
}

namespace
{

enum MenuActions
{
    stopLayout,
    exportAsDot,
    exportAsImage,
    pcOnlyLocal,
    pcOnlyGlobal,
    allInstructionsLocal,
    allInstructionsGlobal,

    // special value
    nActions
};

} // unnamed namespace

void ControlFlowGraphView::contextMenuEvent(QContextMenuEvent* event)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::contextMenuEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    _isMoving = false;

    std::array<QAction*, MenuActions::nActions> actions;

    QMenu popup;
    QGraphicsItem* item = itemAt(event->pos());

    TraceFunction* func = getFunction();

    TraceBasicBlock* bb = nullptr;
    if (item)
    {
        if (item->type() == CanvasParts::Node)
        {
            CFGNode* node = static_cast<CanvasCFGNode*>(item)->node();
            bb = node->basicBlock();

            QMenu* detailsMenu = popup.addMenu(QObject::tr("This basic block"));
            actions[MenuActions::pcOnlyLocal] =
                    addDetailsAction(detailsMenu, QObject::tr("PC only"), node,
                                     CFGExporter::DetailsLevel::pcOnly);
            actions[MenuActions::allInstructionsLocal] =
                    addDetailsAction(detailsMenu, QObject::tr("All instructions"), node,
                                     CFGExporter::DetailsLevel::full);

            popup.addSeparator();
        }
    }

    actions[MenuActions::stopLayout] = addStopLayoutAction(popup);

    popup.addSeparator();

    actions[MenuActions::pcOnlyGlobal] = popup.addAction(QObject::tr("PC only"));
    actions[MenuActions::allInstructionsGlobal] = popup.addAction(QObject::tr("All instructions"));
    addMinimizationMenu(popup);

    popup.addSeparator();

    QMenu* exportMenu = popup.addMenu(QObject::tr("Export Graph"));
    actions[MenuActions::exportAsDot] = exportMenu->addAction(QObject::tr("As DOT file..."));
    actions[MenuActions::exportAsImage] = exportMenu->addAction(QObject::tr("As Image..."));

    popup.addSeparator();

    addLayoutMenu(popup);
    addZoomPosMenu(popup);

    QAction* action = popup.exec(event->globalPos());
    auto index = std::distance(actions.begin(),
                               std::find(actions.begin(), actions.end(), action));

    switch (index)
    {
        case MenuActions::stopLayout:
            stopRendering();
            break;
        case MenuActions::exportAsDot:
        {
            TraceFunction* func = activeFunction();
            if (func)
                CFGExporter::savePrompt(this, func, eventType(), groupType(), _exporter.layout(),
                                        _exporter.detailsMap());
            break;
        }
        case MenuActions::exportAsImage:
            if (_scene)
                exportGraphAsImage();
            break;
        case MenuActions::pcOnlyLocal:
            _exporter.setDetailsLevel(bb, CFGExporter::DetailsLevel::pcOnly);
            _exporter.setMinimalCostPercentage(-1);
            refresh(false);
            break;
        case MenuActions::pcOnlyGlobal:
            _exporter.setDetailsLevel(func, CFGExporter::DetailsLevel::pcOnly);
            _exporter.setMinimalCostPercentage(100);
            refresh(false);
            break;
        case MenuActions::allInstructionsLocal:
            _exporter.setDetailsLevel(bb, CFGExporter::DetailsLevel::full);
            _exporter.setMinimalCostPercentage(-1);
            refresh(false);
            break;
        case MenuActions::allInstructionsGlobal:
            _exporter.setDetailsLevel(func, CFGExporter::DetailsLevel::full);
            _exporter.setMinimalCostPercentage(0);
            refresh(false);
            break;
        default: // practically nActions
            break;
    }
}

void ControlFlowGraphView::exportGraphAsImage()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::exportGraphAsImage" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    assert(_scene);

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    QObject::tr("Export Graph as Image"),
                                                    QString{},
                                                    QObject::tr("Images (*.png *.jpg)"));
    if (!fileName.isEmpty())
    {
        QRect rect = _scene->sceneRect().toRect();
        QPixmap pix{rect.width(), rect.height()};
        QPainter painter{std::addressof(pix)};
        _scene->render(std::addressof(painter));
        pix.save(fileName);
    }
}

namespace
{

CFGEdge* getEdgeToSelect(CFGNode* node, int key)
{
    assert(node);

    switch (key)
    {
        case Qt::Key_Up:
            return node->keyboardPrevEdge();
        case Qt::Key_Down:
            return node->keyboardNextEdge();
        default:
            return nullptr;
    }
}

std::pair<CFGNode*, CFGEdge*> getNodeOrEdgeToSelect(CFGEdge* edge, int key)
{
    assert(edge);

    switch (key)
    {
        case Qt::Key_Up:
            return std::pair{edge->keyboardPrevNode(), nullptr};
        case Qt::Key_Down:
            return std::pair{edge->keyboardNextNode(), nullptr};
        case Qt::Key_Left:
            return std::pair{nullptr, edge->priorVisibleEdge()};
        case Qt::Key_Right:
            return std::pair{nullptr, edge->nextVisibleEdge()};
        default:
            return std::pair{nullptr, nullptr};
    }
}

} // namespace

void ControlFlowGraphView::keyPressEvent(QKeyEvent* e)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::keyPressEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (!_scene)
        e->ignore();
    else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Space)
    {
        if (_selectedNode)
            activated(_selectedNode->basicBlock());
        else if (_selectedEdge && _selectedEdge->branch())
            activated(_selectedEdge->branch());
    }
    else if (!(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)))
    {
        if (_selectedNode)
        {
            int key = _exporter.transformKeyIfNeeded(e->key());
            CFGEdge* edge = getEdgeToSelect(_selectedNode, key);

            if (edge && edge->branch())
                selected(edge->branch());
        }
        else if (_selectedEdge)
        {
            int key = _exporter.transformKeyIfNeeded(e->key());
            auto [node, edge] = getNodeOrEdgeToSelect(_selectedEdge, key);

            if (node && node->basicBlock())
                selected(node->basicBlock());
            else if (edge && edge->branch())
                selected(edge->branch());
        }
    }
    else
        movePointOfView(e);
}

void ControlFlowGraphView::movePointOfView(QKeyEvent* e)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::movePointOfView" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    auto dx = [this]{ return mapToScene(width(), 0) - mapToScene(0, 0); };
    auto dy = [this]{ return mapToScene(0, height()) - mapToScene(0, 0); };

    QPointF center = mapToScene(viewport()->rect().center());
    switch(e->key())
    {
        case Qt::Key_Home:
            centerOn(center + QPointF{-_scene->width(), 0});
            break;
        case Qt::Key_End:
            centerOn(center + QPointF{_scene->width(), 0});
            break;
        case Qt::Key_PageUp:
        {
            QPointF delta = dy();
            centerOn(center + QPointF(-delta.x() / 2.0, -delta.y() / 2.0));
            break;
        }
        case Qt::Key_PageDown:
        {
            QPointF delta = dy();
            centerOn(center + QPointF(delta.x() / 2.0, delta.y() / 2.0));
            break;
        }
        case Qt::Key_Left:
        {
            QPointF delta = dx();
            centerOn(center + QPointF(-delta.x() / 10.0, -delta.y() / 10.0));
            break;
        }
        case Qt::Key_Right:
        {
            QPointF delta = dx();
            centerOn(center + QPointF(delta.x() / 10.0, delta.y() / 10.0));
            break;
        }
        case Qt::Key_Down:
        {
            QPointF delta = dy();
            centerOn(center + QPointF(delta.x() / 10.0, delta.y() / 10.0));
            break;
        }
        case Qt::Key_Up:
        {
            QPointF delta = dy();
            centerOn(center + QPointF(-delta.x() / 10, -delta.y() / 10));
            break;
        }
        default:
            e->ignore();
            break;
    }
}

// Called by QAbstractScrollArea to notify about scrollbar changes
void ControlFlowGraphView::scrollContentsBy(int dx, int dy)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::scrollContentBy" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    // call QGraphicsView implementation
    QGraphicsView::scrollContentsBy(dx, dy);

    QPointF topLeft = mapToScene(QPoint(0, 0));
    QPointF bottomRight = mapToScene(QPoint(width(), height()));

    QRectF z{topLeft, bottomRight};
    _panningView->setZoomRect(z);
}

namespace
{

double calculate_zoom (QSize s, int cWidth, int cHeight)
{
    // first, assume use of 1/3 of width/height (possible larger)
    qreal zoom = (s.width() * cHeight < s.height() * cWidth) ? .33 * s.height() / cHeight
                                                             : .33 * s.width()  / cWidth;

    // fit to widget size
    if (cWidth * zoom > s.width())
        zoom = s.width() / static_cast<double>(cWidth);
    if (cHeight * zoom > s.height())
        zoom = s.height() / static_cast<double>(cHeight);

    // scale to never use full height/width
    zoom *= 0.75;

    // at most a zoom of 1/3
    if (zoom > .33)
        zoom = .33;

    return zoom;
}

} // unnamed namespace

void ControlFlowGraphView::updateSizes(QSize s)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::updateSizes" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (!_scene)
        return;

    if (s == QSize{0, 0})
        s = size();

    // the part of the scene that should be visible
    auto cWidth = static_cast<int>(_scene->width()) - 2*_xMargin + 100;
    auto cHeight = static_cast<int>(_scene->height()) - 2*_yMargin + 100;

    // hide birds eye view if no overview needed
    if (!_data || !_activeItem ||
        ((cWidth < s.width()) && (cHeight < s.height())) ) {
        _panningView->hide();
        return;
    }

    double zoom = calculate_zoom (s, cWidth, cHeight);

    /* Comparing floating point values??? WTF??? */
    if (zoom != _panningZoom) {
        _panningZoom = zoom;

        QTransform m;
        _panningView->setTransform(m.scale(zoom, zoom));

        // make it a little bigger to compensate for widget frame
        _panningView->resize(static_cast<int>(cWidth * zoom) + 4,
                             static_cast<int>(cHeight * zoom) + 4);

        // update ZoomRect in panningView
        scrollContentsBy(0, 0);
    }

    _panningView->centerOn(_scene->width()/2, _scene->height()/2);

    int cvW = _panningView->width();
    int cvH = _panningView->height();
    int x = width() - cvW - verticalScrollBar()->width() - 2;
    int y = height() - cvH - horizontalScrollBar()->height() - 2;

    ZoomPosition zp = _zoomPosition;
    if (zp == ZoomPosition::Auto) {
        auto tlCols = items(QRect(0, 0, cvW, cvH)).count();
        auto trCols = items(QRect(x, 0, cvW, cvH)).count();
        auto blCols = items(QRect(0, y, cvW, cvH)).count();
        auto brCols = items(QRect(x, y, cvW, cvH)).count();
        decltype(tlCols) minCols;

        switch (_lastAutoPosition) {
        case ZoomPosition::TopRight:
            minCols = trCols;
            break;
        case ZoomPosition::BottomLeft:
            minCols = blCols;
            break;
        case ZoomPosition::BottomRight:
            minCols = brCols;
            break;
        default:
        case ZoomPosition::TopLeft:
            minCols = tlCols;
            break;
        }

        if (minCols > tlCols) {
            minCols = tlCols;
            _lastAutoPosition = ZoomPosition::TopLeft;
        }
        if (minCols > trCols) {
            minCols = trCols;
            _lastAutoPosition = ZoomPosition::TopRight;
        }
        if (minCols > blCols) {
            minCols = blCols;
            _lastAutoPosition = ZoomPosition::BottomLeft;
        }
        if (minCols > brCols) {
            minCols = brCols;
            _lastAutoPosition = ZoomPosition::BottomRight;
        }

        zp = _lastAutoPosition;
    }

    QPoint newZoomPos{0, 0};
    switch (zp) {
        break;
    case ZoomPosition::TopRight:
        newZoomPos = QPoint(x, 0);
        break;
    case ZoomPosition::BottomLeft:
        newZoomPos = QPoint(0, y);
        break;
    case ZoomPosition::BottomRight:
        newZoomPos = QPoint(x, y);
        break;
    case ZoomPosition::TopLeft:
    default:
        break;
    }

    if (newZoomPos != _panningView->pos())
        _panningView->move(newZoomPos);

    if (zp == ZoomPosition::Hide)
        _panningView->hide();
    else
        _panningView->show();
}

CostItem* ControlFlowGraphView::canShow(CostItem* i)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::canShow" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (i)
    {
        switch (i->type())
        {
            case ProfileContext::Function:
            case ProfileContext::FunctionCycle:
            case ProfileContext::Call:
            case ProfileContext::BasicBlock:
                return i;
            default:
                break;
        }
    }

    return nullptr;
}

void ControlFlowGraphView::doUpdate(int changeType, bool)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::doUpdate" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (changeType == TraceItemView::selectedItemChanged)
    {
        if (!_scene || !_selectedItem)
            return;

        switch(_selectedItem->type())
        {
            case ProfileContext::BasicBlock:
            {
                CFGNode* node = _exporter.findNode(static_cast<TraceBasicBlock*>(_selectedItem));
                if (node == _selectedNode)
                    return;

                unselectNode();
                unselectEdge();
                selectNode(node);
                break;
            }

            case ProfileContext::Branch:
            {
                auto branch = static_cast<TraceBranch*>(_selectedItem);
                CFGEdge* edge = _exporter.findEdge(branch->instrFrom()->addr(),
                                                   branch->instrTo()->addr());
                if (edge == _selectedEdge)
                    return;

                unselectNode();
                unselectEdge();
                selectEdge(edge);
                break;
            }

            default:
                unselectNode();
                unselectEdge();
                break;
        }

        _scene->update();
    }
    else if (changeType == TraceItemView::groupTypeChanged)
    {
        if (_scene && !_clusterGroups)
        {
            for (auto item : _scene->items())
                if (item->type() == CanvasParts::Node)
                    static_cast<CanvasCFGNode*>(item)->update();

            _scene->update();
        }
    }
    else if (changeType & TraceItemView::dataChanged)
    {
        _exporter.reset(_activeItem, _eventType, _groupType);
        _selectedNode = nullptr;
        _selectedEdge = nullptr;
        refresh();
    }
    else if (changeType != TraceItemView::eventType2Changed)
        refresh();
}

void ControlFlowGraphView::unselectNode()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::unselectNode" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_selectedNode)
    {
        if (_selectedNode->canvasNode())
            _selectedNode->canvasNode()->setSelected(false);

        _selectedNode = nullptr;
    }
}

void ControlFlowGraphView::unselectEdge()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::unselectEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_selectedEdge)
    {
        if (_selectedEdge->canvasEdge())
            _selectedEdge->canvasEdge()->setSelected(false);

        _selectedEdge = nullptr;
    }
}

void ControlFlowGraphView::selectNode(CFGNode* node)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::selectNode" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (node && node->canvasNode())
    {
        _selectedNode = node;

        CanvasCFGNode* cNode = _selectedNode->canvasNode();
        cNode->setSelected(true);

        if (!_isMoving)
            ensureVisible(cNode);
    }
}

void ControlFlowGraphView::selectEdge(CFGEdge* edge)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::selectEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (edge && edge->canvasEdge())
    {
        _selectedEdge = edge;
        _selectedEdge->canvasEdge()->setSelected(true);
    }
}

void ControlFlowGraphView::refresh(bool reset)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::refresh()" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_renderProcess)
        stopRendering();

    _prevSelectedNode = _selectedNode;
    if (_selectedNode)
    {
        QPointF center = _selectedNode->canvasNode()->rect().center();
        _prevSelectedPos = mapFromScene(center);
    }
    else
        _prevSelectedPos = QPoint{-1, -1};

    if (!_data)
    {
        showText(QObject::tr("No trace data"));
        return;
    }
    else if(!_activeItem)
    {
        showText(QObject::tr("No item activated for which to draw the control-flow graph."));
        return;
    }

    switch (_activeItem->type())
    {
        case ProfileContext::BasicBlock:
        case ProfileContext::Branch:
        case ProfileContext::Function:
        case ProfileContext::Call:
            break;
        default:
            showText(QObject::tr("No control-flow graph can be drawn for the active item."));
            return;
    }

    _selectedNode = nullptr;
    _selectedEdge = nullptr;

    if (_scene)
        _scene->clear();
    _unparsedOutput.clear();

    _renderTimer.setSingleShot(true);
    _renderTimer.start(1000);

    _renderProcess = new QProcess{this};

    connect(_renderProcess, &QProcess::readyReadStandardOutput,
            this, &ControlFlowGraphView::readDotOutput);

    connect(_renderProcess, &QProcess::errorOccurred,
            this, &ControlFlowGraphView::dotError);

    auto finishedPtr = static_cast<void (QProcess::*)(int,
                                                      QProcess::ExitStatus)>(&QProcess::finished);
    connect(_renderProcess, finishedPtr,
            this, &ControlFlowGraphView::dotExited);

    QString renderProgram = QStringLiteral("dot");
    QStringList renderArgs{QStringLiteral("-Tplain-ext")};

    _renderProcessCmdLine = renderProgram + QLatin1Char(' ') + renderArgs.join(QLatin1Char(' '));

    qDebug("ControlFlowGraphView::refresh: Starting process %p, \'%s\'",
           _renderProcess, qPrintable(_renderProcessCmdLine));

    QProcess* process = _renderProcess;
    process->start(renderProgram, renderArgs);
    if (reset)
        _exporter.reset(_selectedItem ? _selectedItem : _activeItem, _eventType, _groupType);
    _exporter.writeDot(CFGExporter::DumpType::internal, process);
    process->closeWriteChannel();
}

void ControlFlowGraphView::clear()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::clear" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (!_scene)
        return;

    _panningView->setScene(nullptr);
    setScene(nullptr);
    delete _scene;
    _scene = nullptr;
}

void ControlFlowGraphView::showText(const QString& text)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::showText" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    clear();
    _renderTimer.stop();

    _scene = new QGraphicsScene;
    _scene->addSimpleText(text);

    centerOn(0, 0);
    setScene(_scene);
    _scene->update();
    _panningView->hide();
}

QAction* ControlFlowGraphView::addZoomPosAction(QMenu* menu, const QString& descr, ZoomPosition pos)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addZoomPosAction" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QAction* a = menu->addAction(descr);

    a->setData(static_cast<int>(pos));
    a->setCheckable(true);
    a->setChecked(_zoomPosition == pos);

    return a;
}

QAction* ControlFlowGraphView::addLayoutAction(QMenu* menu, const QString& descr,
                                               CFGExporter::Layout layout)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addLayoutAction" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QAction* a = menu->addAction(descr);

    a->setData(static_cast<int>(layout));
    a->setCheckable(true);
    a->setChecked(_exporter.layout() == layout);

    return a;
}

QAction* ControlFlowGraphView::addStopLayoutAction(QMenu& menu)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addStopLayoutAction" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_renderProcess)
    {
        QAction* stopLayout_ = menu.addAction(QObject::tr("Stop Layouting"));
        menu.addSeparator();

        return stopLayout_;
    }
    else
        return nullptr;
}

QAction* ControlFlowGraphView::addDetailsAction(QMenu* menu, const QString& descr, CFGNode* node,
                                                CFGExporter::DetailsLevel level)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addDetailsAction" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QAction* a = menu->addAction(descr);

    a->setData(static_cast<int>(level));
    a->setCheckable(true);
    a->setChecked(_exporter.detailsLevel(node->basicBlock()) == level);

    return a;
}

QAction* ControlFlowGraphView::addMinimizationAction(QMenu* menu, const QString& descr,
                                                     double percentage)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addMinimizationAction" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QAction* a = menu->addAction(descr);

    a->setData(percentage);
    a->setCheckable(true);
    a->setChecked(percentage == _exporter.minimalCostPercentage());
    if (percentage == -1)
        a->setEnabled(false);

    return a;
}

QMenu* ControlFlowGraphView::addZoomPosMenu(QMenu& menu)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addZoomPosMenu" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QMenu* submenu = menu.addMenu(QObject::tr("Birds-eye View"));

    addZoomPosAction(submenu, QObject::tr("Top Left"), ZoomPosition::TopLeft);
    addZoomPosAction(submenu, QObject::tr("Top Right"), ZoomPosition::TopRight);
    addZoomPosAction(submenu, QObject::tr("Bottom Left"), ZoomPosition::BottomLeft);
    addZoomPosAction(submenu, QObject::tr("Bottom Right"), ZoomPosition::BottomRight);
    addZoomPosAction(submenu, QObject::tr("Automatic"), ZoomPosition::Auto);
    addZoomPosAction(submenu, QObject::tr("Hide"), ZoomPosition::Hide);

    connect(submenu, &QMenu::triggered,
            this, &ControlFlowGraphView::zoomPosTriggered);

    return submenu;
}

QMenu* ControlFlowGraphView::addLayoutMenu(QMenu& menu)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addLayoutMenu" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QMenu* submenu = menu.addMenu(QObject::tr("Layout"));

    addLayoutAction(submenu, QObject::tr("Top to Down"), CFGExporter::Layout::TopDown);
    addLayoutAction(submenu, QObject::tr("Left to Right"), CFGExporter::Layout::LeftRight);

    connect(submenu, &QMenu::triggered,
            this, &ControlFlowGraphView::layoutTriggered);

    return submenu;
}

QMenu* ControlFlowGraphView::addMinimizationMenu(QMenu& menu)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addMinimizationMenu" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QMenu* submenu = menu.addMenu(QObject::tr("Min. basic block cost"));

    addMinimizationAction(submenu, QObject::tr("Undefined"), -1);
    submenu->addSeparator();
    for (auto percentage : {0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0})
        addMinimizationAction(submenu, QObject::tr("%1%").arg(percentage), percentage);

    connect(submenu, &QMenu::triggered,
            this, &ControlFlowGraphView::minimizationTriggered);

    return submenu;
}

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

void CFGNode::clearEdges()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::clearEdges()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    _trueEdge = _falseEdge = nullptr;
    _predecessors.clear();
}

void CFGNode::sortPredecessorEdges()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::sortPredecessorEdges()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    auto edgeComp = [](const CFGEdge* ge1, const CFGEdge* ge2)
    {
        auto ce1 = ge1->canvasEdge();
        auto ce2 = ge2->canvasEdge();

        if (!ce1 && !ce2)
            return ce1 < ce2;
        else if (!ce1)
            return true;
        else if (!ce2)
            return false;
        else
        {
            auto& p1 = ce1->controlPoints();
            auto& p2 = ce2->controlPoints();
            QPoint d1 = p1.point(1) - p1.point(0);
            QPoint d2 = p2.point(1) - p2.point(0);

            auto angle1 = std::atan2(static_cast<double>(d1.y()), static_cast<double>(d1.x()));
            auto angle2 = std::atan2(static_cast<double>(d2.y()), static_cast<double>(d2.x()));

            return angle1 < angle2;
        }
    };

    std::sort(_predecessors.begin(), _predecessors.end(), edgeComp);
}

void CFGNode::setTrueEdge(CFGEdge* e)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::setTrueEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (e)
    {
        _trueEdge = e;
        _trueEdge->setPredecessorNode(this);
    }
}

void CFGNode::setFalseEdge(CFGEdge* e)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::setFalseEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (e)
    {
        _falseEdge = e;
        _falseEdge->setPredecessorNode(this);
    }
}

void CFGNode::selectSuccessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::selectSuccessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (edge == _trueEdge)
        _lastSuccessorIndex = trueIndex;
    else if (edge == _falseEdge)
        _lastSuccessorIndex = falseIndex;
    else
        _lastSuccessorIndex = noIndex;

    _lastFromPredecessor = false;
}

void CFGNode::selectPredecessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::selectPredecessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    _lastPredecessorIndex = _predecessors.indexOf(edge);
    _lastFromPredecessor = true;
}

void CFGNode::addPredecessor(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::addPredecessor()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (edge)
        _predecessors.append(edge);
}

void CFGNode::addUniquePredecessor(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::addUniquePredecessor()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (edge && (_predecessors.count(edge) == 0))
        _predecessors.append(edge);
}

double CFGNode::successorCostSum() const
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::successorCostSum()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    return (_trueEdge ? _trueEdge->cost : 0.0) + (_falseEdge ? _falseEdge->cost : 0.0);
}

double CFGNode::successorCountSum() const
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::successorCountSum()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    return (_trueEdge ? _trueEdge->count : 0.0) + (_falseEdge ? _falseEdge->count : 0.0);
}

double CFGNode::predecessorCostSum() const
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::predecessorCostSum()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    return std::accumulate(_predecessors.begin(), _predecessors.end(), 0.0,
                           [](double val, CFGEdge* e){ return val + e->cost; });
}

double CFGNode::predecessorCountSum() const
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::predecessorCountSum()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    return std::accumulate(_predecessors.begin(), _predecessors.end(), 0.0,
                           [](double val, CFGEdge* e){ return val + e->count; });
}

CFGEdge* CFGNode::visibleSuccessorEdge()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::visibleSuccessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (_trueEdge && _falseEdge)
    {
        if (_trueEdge->isVisible() && _falseEdge->isVisible())
        {
            return std::max(_trueEdge, _falseEdge,
                            [](CFGEdge* e1, CFGEdge* e2){ return e1->cost < e2->cost; });
        }
        else if (_trueEdge->isVisible())
            return _trueEdge;
        else if (_falseEdge->isVisible())
            return _falseEdge;
        else
            return nullptr;
    }
    else if (_trueEdge)
        return _trueEdge->isVisible() ? _trueEdge: nullptr;
    else if (_falseEdge)
        return _falseEdge->isVisible() ? _falseEdge : nullptr;
    else
        return nullptr;
}

CFGEdge* CFGNode::visiblePredecessorEdge()
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::visiblePredecessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    auto edge = _predecessors.value(_lastPredecessorIndex);

    if (edge && !edge->isVisible())
        edge = nullptr;

    if (!edge)
    {
        double maxCost = 0.0;
        CFGEdge* maxEdge = nullptr;

        for (auto i = 0; i < _predecessors.size(); ++i)
        {
            edge = _predecessors[i];

            if (edge->isVisible() && edge->cost > maxCost)
            {
                maxEdge = edge;
                maxCost = maxEdge->cost;
                _lastPredecessorIndex = i;
            }
        }

        edge = maxEdge;
    }

    return edge;
}

CFGEdge* CFGNode::nextVisibleSuccessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::nextVisibleSuccessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (edge)
    {
        if (edge == _trueEdge && _falseEdge && _falseEdge->isVisible())
        {
            _lastSuccessorIndex = falseIndex;
            return _falseEdge;
        }
    }
    else
    {
        if (_lastSuccessorIndex == noIndex)
        {
            if (_trueEdge && _trueEdge->isVisible())
            {
                _lastSuccessorIndex = trueIndex;
                return _trueEdge;
            }
            else if (_falseEdge && _falseEdge->isVisible())
            {
                _lastSuccessorIndex = falseIndex;
                return _falseEdge;
            }
        }
        else if (_lastSuccessorIndex == trueIndex)
        {
            if (_falseEdge && _falseEdge->isVisible())
            {
                _lastSuccessorIndex = falseIndex;
                return _falseEdge;
            }
        }
    }

    return nullptr;
}

CFGEdge* CFGNode::nextVisiblePredecessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::nextVisiblePredecessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    auto shift = edge ? _predecessors.indexOf(edge) : _lastPredecessorIndex;
    auto begin = std::next(_predecessors.begin(), shift + 1);
    auto end = _predecessors.end();

    auto it = std::find_if(begin, end, [](CFGEdge* e){ return e->isVisible(); });

    if (it == end)
        return nullptr;
    else
    {
        _lastPredecessorIndex = std::distance(begin, it);
        return *it;
    }
}

CFGEdge* CFGNode::priorVisibleSuccessorEdge(CFGEdge* edge)
{
    #ifdef CFGNODE_DEBUG
    qDebug() << "\033[1;31m" << "CFGNode::priorVisibleSuccessorEdge()" << "\033[0m";
    #endif // CFGNODE_DEBUG

    if (edge)
    {
        if (edge == _falseEdge && _trueEdge && _trueEdge->isVisible())
        {
            _lastSuccessorIndex = trueIndex;
            return _trueEdge;
        }
    }
    else
    {
        if (_lastSuccessorIndex == noIndex)
        {
            if (_falseEdge && _falseEdge->isVisible())
            {
                _lastSuccessorIndex = falseIndex;
                return _falseEdge;
            }
            else if (_trueEdge && _trueEdge->isVisible())
            {
                _lastSuccessorIndex = trueIndex;
                return _trueEdge;
            }
        }
        else if (_lastSuccessorIndex == falseIndex)
        {
            if (_trueEdge && _trueEdge->isVisible())
            {
                _lastSuccessorIndex = trueIndex;
                return _trueEdge;
            }
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

CFGNode* CFGEdge::cachedFromNode()
{
    if (_fromNode)
    {
        _lastFromPredecessor = true;
        _fromNode->selectPredecessorEdge(this);
    }

    return _fromNode;
}

CFGNode* CFGEdge::cachedToNode()
{
    if (_toNode)
    {
        _lastFromPredecessor = false;
        _toNode->selectSuccessorEdge(this);
    }

    return _fromNode;
}

const TraceBasicBlock* CFGEdge::from() const
{
    return _fromNode ? _fromNode->basicBlock() : nullptr;
}

TraceBasicBlock* CFGEdge::from()
{
    return _fromNode ? _fromNode->basicBlock() : nullptr;
}

TraceBasicBlock* CFGEdge::cachedFrom()
{
    auto bb = from();

    if (bb)
    {
        _lastFromPredecessor = true;
        if (_fromNode)
            _fromNode->selectPredecessorEdge(this);
    }

    return bb;
}

const TraceBasicBlock* CFGEdge::to() const
{
    return _toNode ? _toNode->basicBlock() : nullptr;
}

TraceBasicBlock* CFGEdge::to()
{
    return _toNode ? _toNode->basicBlock() : nullptr;
}

TraceBasicBlock* CFGEdge::cachedTo()
{
    auto bb = to();

    if (bb)
    {
        _lastFromPredecessor = false;
        if (_fromNode)
            _fromNode->selectSuccessorEdge(this);
    }

    return bb;
}

CFGEdge* CFGEdge::nextVisibleEdge()
{
    CFGEdge* edge = nullptr;

    if (_lastFromPredecessor && _fromNode)
    {
        edge = _fromNode->nextVisibleSuccessorEdge(this);
        if (!edge && _toNode)
            edge = _toNode->nextVisiblePredecessorEdge(this);
    }
    else if (_toNode)
    {
        edge = _toNode->nextVisiblePredecessorEdge(this);
        if (!edge && _fromNode)
            edge = _fromNode->nextVisibleSuccessorEdge(this);
    }

    return edge;
}

CFGEdge* CFGEdge::priorVisibleEdge()
{
    CFGEdge* edge = nullptr;

    if (_lastFromPredecessor && _fromNode)
    {
        edge = _fromNode->priorVisibleSuccessorEdge(this);
        if (!edge && _toNode)
            edge = _toNode->priorVisiblePredecessorEdge(this);
    }
    else if (_toNode)
    {
        edge = _toNode->priorVisiblePredecessorEdge(this);
        if (!edge && _fromNode)
            edge = _fromNode->priorVisibleSuccessorEdge(this);
    }

    return edge;
}

QString CFGEdge::prettyName() const
{
    auto bbFrom = from();

    QString name;
    if (bbFrom)
    {
        name = QObject::tr("Branch from %1").arg(bbFrom->prettyName());

        auto bbTo = to();
        if (bbTo)
            name += QObject::tr(" to %1").arg(bbTo->prettyName());
    }
    else
        name = QObject::tr("(unknown branch)");

    return name;
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
}

CFGExporter::~CFGExporter()
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::~CFGExporter()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    delete _tmpFile;
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

const CFGEdge* CFGExporter::findEdge(TraceBasicBlock* bb1, TraceBasicBlock* bb2) const
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::findEdge()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    auto it = _edgeMap.find(std::make_pair(bb1, bb2));
    return (it == _edgeMap.end()) ? nullptr : std::addressof(*it);
}

CFGEdge* CFGExporter::findEdge(TraceBasicBlock* bb1, TraceBasicBlock* bb2)
{
    return const_cast<CFGEdge*>(static_cast<const CFGExporter*>(this)->findEdge(bb1, bb2));
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
        switch (i->type())
        {
            case ProfileContext::Function:
            {
                auto& BBs = static_cast<TraceFunction*>(i)->basicBlocks();
                assert(!BBs.empty());
                _item = BBs.front();
                break;
            }
            case ProfileContext::Call:
            {
                auto caller = static_cast<TraceCall*>(i)->caller(true);
                auto& BBs = caller->basicBlocks();
                assert(!BBs.empty());
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
        node.sortPredecessorEdges();
}

bool CFGExporter::writeDot(QIODevice* device)
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
        dumpEdges(*stream);

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

    switch(_item->type())
    {
        case ProfileContext::Function:
        case ProfileContext::FunctionCycle:
        {
            auto& BBs = static_cast<TraceFunction*>(_item)->basicBlocks();
            assert(!BBs.empty());
            _item = BBs.front();
            break;
        }
        case ProfileContext::Call:
        {
            auto f = static_cast<TraceCall*>(_item)->caller(false);
            auto& BBs = f->basicBlocks();
            assert(!BBs.empty());
            _item = BBs.front();
            break;
        }
        case ProfileContext::BasicBlock:
            break;
        default:
            assert(!"Unsupported type of item");
    }

    auto bb = static_cast<TraceBasicBlock*>(_item);

    buildNode(bb);

    return fillInstrStrings(bb->function());
}

CFGNode* CFGExporter::buildNode(TraceBasicBlock* bb)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::buildNode()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    assert(bb);

    std::pair key{bb->firstAddr(), bb->lastAddr()};
    auto nodeIt = _nodeMap.find(key);

    if (nodeIt == _nodeMap.end())
    {
        nodeIt = _nodeMap.insert(key, CFGNode{bb});
        auto nodePtr = std::addressof(*nodeIt);

        nodeIt->setTrueEdge(buildEdge(nodePtr, std::addressof(bb->trueBranch())));
        nodeIt->setFalseEdge(buildEdge(nodePtr, std::addressof(bb->falseBranch())));
    }

    return std::addressof(*nodeIt);
}

CFGEdge* CFGExporter::buildEdge(CFGNode* fromNode, TraceBranch* branch)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::buildEdge()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    assert(fromNode);
    assert(branch);

    TraceBasicBlock* to = branch->toBB();
    if (to)
    {
        TraceBasicBlock* from = branch->fromBB();

        std::pair key{from, to};
        auto edgeIt = _edgeMap.find(key);

        if (edgeIt == _edgeMap.end())
        {
            edgeIt = _edgeMap.insert(key, CFGEdge{branch});
            edgeIt->setPredecessorNode(fromNode);
            edgeIt->setSuccessorNode((to == from) ? fromNode : buildNode(to));
        }

        return std::addressof(*edgeIt);
    }
    else
        return nullptr;
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

    std::pair<QString, QMap<Addr, QString>> getInstrStrings();

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

    if (!runObjdump(func))
        throw std::runtime_error{"Error while running objdump"};
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

        auto margin = _isArm ? 4 : 20;

        auto args = QStringList{"-C", "-d"};
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

std::pair<QString, QMap<Addr, QString>> ObjdumpParser::getInstrStrings()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::getInstrStrings()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    QMap<Addr, QString> instrStrings;

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
            instrStrings.insert(_objAddr, mnemonic + " " + operands);
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
        auto readBytes = _objdump.readLine(_line.absData(), _line.capacity());
        if (readBytes <= 0)
        {
            _objAddr = Addr{0};
            break;
        }
        else
        {
            _objdumpLineno++;
            if (readBytes == static_cast<qsizetype>(_line.capacity()))
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
    auto digits = addr.set(_line.relData());
    _line.advance(digits);

    return (digits == 0 || _line.elem() != ':') ? Addr{0} : addr;
}

QString ObjdumpParser::parseEncoding()
{
    #ifdef OBJDUMP_PARSER_DEBUG
    qDebug() << "\033[1;31m" << "ObjdumpParser::parseEncoding()" << "\033[0m";
    #endif // OBJDUMP_PARSER_DEBUG

    _line.skipWhitespaces();
    auto start = _line.getPos();

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
    auto start = _line.getPos();

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

    auto operandsPos = _line.relData();
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
        TraceBasicBlock* bb = node.basicBlock();

        ts << QStringLiteral("  b%1b%2 [shape=record, label=\"")
                            .arg(bb->firstAddr().toString())
                            .arg(bb->lastAddr().toString());

        if (_layout == Layout::TopDown)
            ts << '{';

        auto lastInstrIt = std::prev(bb->end());

        auto i = 0;
        for (auto it = bb->begin(); it != lastInstrIt; ++it, ++i)
        {
            TraceInstr* instr = *it;
            if (bb->existsJumpToInstr(instr))
                ts << QStringLiteral("<I%1>").arg(instr->addr().toString());

            ts << *std::next(node.begin(), i) << " | ";
        }

        ts << QStringLiteral("<I%1>").arg((*lastInstrIt)->addr().toString())
           << *std::prev(node.end());

        if (_layout == Layout::TopDown)
            ts << '}';

        ts << "\"]\n";
    }
}

void CFGExporter::dumpEdges(QTextStream& ts)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::dumpEdges()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    for (auto& edge : _edgeMap)
    {
        TraceBranch* br = edge.branch();
        assert(br);

        TraceBasicBlock* fromBB = br->fromBB();
        assert(fromBB);

        auto bbFromFirstAddr = fromBB->firstAddr().toString();
        auto bbFromLastAddr = fromBB->lastAddr().toString();

        TraceBasicBlock* toBB = br->toBB();
        assert(toBB);

        auto bbToFirstAddr = toBB->firstAddr().toString();
        auto bbToLastAddr = toBB->lastAddr().toString();

        switch (br->brType())
        {
            case TraceBranch::Type::true_:
            case TraceBranch::Type::unconditional:
            {
                const char* color = (br->brType() == TraceBranch::Type::true_) ? "blue" : "black";

                ts << QStringLiteral("  b%1b%2:I%3:w -> b%4b%5")
                                    .arg(bbFromFirstAddr).arg(bbFromLastAddr).arg(bbFromLastAddr)
                                    .arg(bbToFirstAddr).arg(bbToLastAddr);

                if (br->isCycle())
                    ts << QStringLiteral(":I%1:w [constraint=false, color=%2]\n")
                                        .arg(br->toInstr()->addr().toString()).arg(color);
                else if (br->isBranchInside())
                    ts << QStringLiteral(":I%1 [color=%2]\n")
                                        .arg(br->toInstr()->addr().toString()).arg(color);
                else
                    ts << QStringLiteral(":n [color=%1]\n").arg(color);

                break;
            }

            case TraceBranch::Type::false_:
                ts << QStringLiteral("  b%1b%2:I%3:e -> b%4b%5:n [color=red]\n")
                                    .arg(bbFromFirstAddr).arg(bbFromLastAddr).arg(bbFromLastAddr)
                                    .arg(bbToFirstAddr).arg(bbToLastAddr);
                break;

            default:
                assert(false);
                break;
        }

        #if 0
        ts << QStringLiteral("  B%1 -> B%2 [weight=%3")
                            .arg(reinterpret_cast<std::ptrdiff_t>(edge.from()), 0, 16)
                            .arg(reinterpret_cast<std::ptrdiff_t>(edge.to()), 0, 16)
                            .arg(static_cast<long>(std::log(std::log(edge.cost))));

        if (_detailLevel == DetailLevel::avgDetails)
            ts << QStringLiteral(",label=\"%1 (%2x)\"")
                                .arg(SubCost{edge.cost}.pretty())
                                .arg(SubCost{edge.count}.pretty());
        else if (_detailLevel == DetailLevel::moreDetails)
            ts << QStringLiteral(",label=\"%3\\n%4 x\"")
                                .arg(SubCost{edge.cost}.pretty())
                                .arg(SubCost{edge.count}.pretty());

        ts << QStringLiteral("];\n");
        #endif
    }
}

CFGNode* CFGExporter::toCFGNode(QString s)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::toBasicBlock()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (s[0] == 'b')
    {
        auto i = s.indexOf('b', 1);
        if (i != -1)
        {
            bool ok;
            auto from = s.mid(1, i - 1).toULongLong(&ok, 16);
            if (ok)
            {
                auto to = s.mid(i + 1).toULongLong(&ok, 16);
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
                             EventType* eventType, ProfileContext::Type groupType, Layout layout)
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

        auto mime = saveDialog.selectedMimeTypeFilter();
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

        bool wrote = ge.writeDot();
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

#if 0
namespace
{

double calculateTotalInclusiveCost (ControlFlowGraphView* view)
{
    ProfileCostArray* totalCost;
    if (GlobalConfig::showExpanded())
    {
        auto activeBB = view->activeBasicBlock();
        if (activeBB)
        {
             if (activeBB->cycle())
                totalCost = activeBB->cycle()->inclusive();
            else
                totalCost = activeBB->inclusive();
        }
        else
            totalCost = static_cast<ProfileCostArray*>(view->activeItem());
    }
    else
        totalCost = static_cast<TraceItemView*>(view)->data();

    return totalCost->subCost(view->eventType());
}

} // unnamed namespace
#endif

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

    #if 0
    if (_node->basicBlock())
        setText(0, _node->basicBlock()->prettyName()); // set BB's name

    auto total = calculateTotalInclusiveCost(_view);
    auto inclPercentage = 100.0 * _node->incl / total;

    // set inclusive cost
    if (GlobalConfig::showPercentage())
        setText(1, QStringLiteral("%1 %")
                                 .arg(inclPercentage, 0, 'f', GlobalConfig::percentPrecision()));
    else
        setText(1, SubCost(_node->incl).pretty());

    // set percentage bar
    setPixmap(1, percentagePixmap(25, 10, static_cast<int>(inclPercentage + 0.5), Qt::blue, true));

    // set tool tip (balloon help) with the name of a basic block and percentage
    setToolTip(QStringLiteral("%1 (%2)").arg(text(0)).arg(text(1)));
    #endif
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

    QRectF rectangle = rect();

    if (StoredDrawParams::selected())
    {
        QPen pen{Qt::darkGreen};
        pen.setWidth(2);
        p->setPen(pen);

        p->drawRect(rectangle);
        p->setPen(Qt::black);
    }
    else
    {
        p->setPen(Qt::black);
        p->drawRect(rectangle);
    }

    auto step = rectangle.height() / _node->instrNumber();

    auto topLineY = rectangle.y();

    auto instrIt = _node->begin();
    p->drawText(rectangle.x(), topLineY, rectangle.width(), step,
                Qt::AlignCenter, *instrIt);
    topLineY += step;
    ++instrIt;

    for (auto ite = _node->end(); instrIt != ite; ++instrIt)
    {
        p->drawText(rectangle.x(), topLineY, rectangle.width(), step,
                    Qt::AlignCenter, *instrIt);
        p->drawLine(rectangle.x(), topLineY,
                    rectangle.x() + rectangle.width(), topLineY);

        topLineY += step;
    }
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

    auto e = _ce->edge();
    if (!e)
        return;

    #if 0
    setPosition(1, DrawParams::BottomCenter);

    auto total = calculateTotalInclusiveCost(_view);
    auto inclPercentage = 100.0 * e->cost / total;

    _percentage = std::min(inclPercentage, 100.0);

    if (GlobalConfig::showPercentage())
        setText(1, QStringLiteral("%1 %")
                                 .arg(inclPercentage, 0, 'f', GlobalConfig::percentPrecision()));
    else
        setText(1, SubCost(e->cost).pretty());

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

    if (e->to() && e->from() == e->to())
    {
        QFontMetrics fm{font()};
        auto pixmap = QIcon::fromTheme(QStringLiteral("edit-undo")).pixmap(fm.height());
        setPixmap(pixPos, pixmap);
    }
    else
        setPixmap(pixPos, percentagePixmap(25, 10, static_cast<int>(inclPercentage + 0.5),
                                           Qt::blue, true));
    #endif
}

void CanvasCFGEdgeLabel::paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget*)
{
    #ifdef CANVASCFGEDGELABEL_DEBUG
    qDebug() << "\033[1;31m" << "CanvasCFGEdgeLabel::paint()" << "\033[0m";
    #endif // CANVASCFGEDGELABEL_DEBUG

#if QT_VERSION >= 0x040600
    if (option->levelOfDetailFromTransform(p->transform()) < 0.5)
        return;
#else
    if (option->levelOfDetail < 0.5)
        return;
#endif

    RectDrawing drawer{rect().toRect()};

    #if 0
    drawer.drawField(p, 0, this);
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
    p->setBrush(_ce->isSelected() ? Qt::red : Qt::black);
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
        auto tip = QStringLiteral("%1 (%2)").arg(l->text(0)).arg(l->text(1));

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
    for (auto i = 1; i < p.size(); i += 3)
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
    auto levelOfDetail = option->levelOfDetailFromTransform(p->transform());
#else
    auto levelOfDetail = option->levelOfDetail;
#endif

    auto mypen = pen();

    // paint whole edge black
    mypen.setWidthF(1.0 / levelOfDetail * _thickness);
    p->setPen(mypen);
    p->drawPath(path());

    if (isSelected())
    {
        // paint inner half of edge red
        mypen.setColor(Qt::red);
        mypen.setWidthF(1.0 / levelOfDetail * _thickness / 2.0);
        p->setPen(mypen);
        p->drawPath(path());
    }
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

void ControlFlowGraphView::zoomRectMoved(qreal dx, qreal dy)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::zoomRectVoved" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    //FIXME if (leftMargin()>0) dx = 0;
    //FIXME if (topMargin()>0) dy = 0;

    auto hBar = horizontalScrollBar();
    auto vBar = verticalScrollBar();
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

    auto err = QObject::tr("No graph available because the layouting process failed.\n");
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

    auto [activeNode, activeEdge] = parseDot();

    checkSceneAndActiveItems(activeNode, activeEdge);

    _exporter.sortEdges();

    _panningZoom = 0.0;
    _panningView->setScene(_scene);
    setScene(_scene);

    updateSelectedNodeOrEdge(activeNode, activeEdge);
    centerOnSelectedNodeOrEdge();

    updateSizes();

    _scene->update();
    viewport()->setUpdatesEnabled(true);

    #if 0 // I've got the strongest feeling this piece of code is useless
    delete _renderProcess;
    _renderProcess = nullptr;
    #endif
}

std::pair<CFGNode*, CFGEdge*> ControlFlowGraphView::parseDot()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::parseDot" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QTextStream dotStream{std::addressof(_unparsedOutput), QIODevice::ReadOnly};
    _scaleY = 8 + 3 * fontMetrics().height();

    QString cmd;
    CFGNode* activeNode = nullptr;
    CFGEdge* activeEdge = nullptr;
    for (auto lineno = 1; ; lineno++)
    {
        auto line = dotStream.readLine();

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
                activeNode = parseNode(activeNode, lineStream);
            else if (cmd == QLatin1String("edge"))
                activeEdge = parseEdge(activeEdge, lineStream, lineno);
        }
    }

    return std::pair{activeNode, activeEdge};
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

CFGNode* ControlFlowGraphView::parseNode(CFGNode* activeNode, QTextStream& lineStream)
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

        qreal w = _scaleX * nodeWidth.toDouble();
        qreal h = _scaleY * nodeHeight.toDouble();

        auto rItem = new CanvasCFGNode{this, node, xx - w / 2, yy - h / 2, w, h};
        rItem->setZValue(1.0);
        rItem->show();

        node->setCanvasNode(rItem);

        _scene->addItem(rItem);

        if (node->basicBlock() == activeItem())
            activeNode = node;

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

    return activeNode;
}

namespace
{

CFGEdge* getEdge(CFGNode* predecessor, CFGNode* successor)
{
    if (predecessor && successor)
    {
        auto te = predecessor->trueEdge();
        if (te && te->toNode() == successor)
            return te;
        else
        {
            auto fe = predecessor->falseEdge();
            return (fe && fe->toNode() == successor) ? fe : nullptr;
        }
    }
    else
        return nullptr;
}

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

CFGEdge* ControlFlowGraphView::parseEdge(CFGEdge* activeEdge, QTextStream& lineStream, int lineno)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::parseEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString node1Name, node2Name;
    lineStream >> node1Name >> node2Name;

    CFGEdge* edge = getEdge(_exporter.toCFGNode(node1Name), _exporter.toCFGNode(node2Name));

    if (!edge)
    {
        qDebug() << "Unknown edge \'" << node1Name << "\'-\'" << node2Name << "\' from dot ("
                << _exporter.filename() << ":" << lineno << ")";

        return activeEdge;
    }

    edge->setVisible(true);

    int nPoints;
    lineStream >> nPoints;
    assert(nPoints > 1);

    QPolygon poly(nPoints);

    for (auto i = 0; i != nPoints; ++i)
    {
        if (lineStream.atEnd())
        {
            qDebug("ControlFlowGraphView: Can not read %d spline nPoints (%s:%d)",
                    nPoints, qPrintable(_exporter.filename()), lineno);
            return nullptr;
        }

        auto [xx, yy] = calculateSizes(lineStream);
        poly.setPoint(i, xx, yy);
    }

    QColor arrowColor = getArrowColor(edge);

    auto sItem = createEdge(edge, poly, arrowColor);

    _scene->addItem(sItem);
    _scene->addItem(createArrow(sItem, poly, arrowColor));

    #if 0
    if (edge->branch() == selectedItem())
    {
        _selectedEdge = edge;
        sItem->setSelected(true);
    }
    else
        sItem->setSelected(edge == _selectedEdge);

    if (edge->branch() == activeItem())
        activeEdge = edge;
    #endif

    // Useless till costs of branches aren't calculated
    #if 0
    if (!lineStream.atEnd())
    {
        lineStream.skipWhiteSpace();

        QChar c;
        lineStream >> c;

        QString label;
        if (c == '\"')
        {
            lineStream >> c;
            while (!c.isNull() && c != '\"')
            {
                label.append(c);
                lineStream >> c;
            }
        }
        else
        {
            lineStream >> label;
            label.prepend(c);
        }

        auto [xx, yy] = calculateSizes(lineStream);
        auto lItem = new CanvasCFGEdgeLabel{this, sItem,
                                            static_cast<qreal>(xx - 50),
                                            static_cast<qreal>(yy - _detailLevel * 10),
                                            100.0,
                                            static_cast<qreal>(_detailLevel * 20)};
        _scene->addItem(lItem);
        lItem->setZValue(1.5);
        sItem->setLabel(lItem);

        if (_exporter.detailLevel() > 0)
            lItem->show();
    }
    #endif

    return activeEdge;
}

void ControlFlowGraphView::checkSceneAndActiveItems(CFGNode* activeNode, CFGEdge* activeEdge)
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
    else if (!activeNode && !activeEdge)
    {
        auto message = QObject::tr("There is no control-flow graph available for basic block\n"
                                   "    \'%1\'\n"
                                   "because it has no cost of the selected event type.")
                                  .arg(_activeItem->name());
        _scene->addSimpleText(message);
        centerOn(0, 0);
    }
}

void ControlFlowGraphView::updateSelectedNodeOrEdge(CFGNode* activeNode, CFGEdge* activeEdge)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::updateSelectedNodeOrEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if ((!_selectedNode || !_selectedNode->canvasNode()) &&
        (!_selectedEdge || !_selectedEdge->canvasEdge()))
    {
        if (activeNode)
        {
            _selectedNode = activeNode;

            auto cn = _selectedNode->canvasNode();
            cn->setSelected(true);
        }
        else if (activeEdge)
        {
            _selectedEdge = activeEdge;
            _selectedEdge->canvasEdge()->setSelected(true);
        }
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
        if (_selectedEdge->fromNode())
            sNode = _selectedEdge->fromNode()->canvasNode();

        if (!sNode && _selectedEdge->toNode())
            sNode = _selectedEdge->toNode()->canvasNode();
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

#if 0
void ControlFlowGraphView::predecessorDepthTriggered(QAction* a)
{
    _maxCallerDepth = a->data().toInt(nullptr);
    refresh();
}

void ControlFlowGraphView::successorDepthTriggered(QAction* a)
{
    _maxCalleeDepth = a->data().toInt(nullptr);
    refresh();
}

void ControlFlowGraphView::nodeLimitTriggered(QAction* a)
{
    _funcLimit = a->data().toInt(nullptr);
    refresh();
}

void ControlFlowGraphView::branchLimitTriggered(QAction* a)
{
    _callLimit = a->data().toInt(nullptr);
    refresh();
}
#endif

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
    refresh();
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
            [[maybe_unused]] CFGEdge* edge = static_cast<CanvasCFGEdge*>(item)->edge();

            #if 0
            if (edge->branch())
                (this->*func)(edge->branch());
            #endif
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
        auto hBar = horizontalScrollBar();
        auto vBar = verticalScrollBar();

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
    activateBasicBlock,
    #if 0
    activateBranch,
    #endif
    stopLayout,
    exportAsDot,
    exportAsImage,

    // special value
    nActions
};

TraceBasicBlock* addNodesOrEdgesAction(QMenu& popup, QGraphicsItem* item,
                           std::array<QAction*, MenuActions::nActions>& actions)
{
    TraceBasicBlock* bb = nullptr;
    #if 0
    TraceBranch* branch = nullptr;
    #endif

    if (item)
    {
        if (item->type() == CanvasParts::Node)
        {
            bb = static_cast<CanvasCFGNode*>(item)->node()->basicBlock();
            actions[MenuActions::activateBasicBlock] =
                    popup.addAction(QObject::tr("Go to \'%1\'")
                                    .arg(GlobalConfig::shortenSymbol(bb->prettyName())));

            popup.addSeparator();
        }
        else
        {
            if (item->type() == CanvasParts::EdgeLabel)
                item = static_cast<CanvasCFGEdgeLabel*>(item)->canvasEdge();
            else if (item->type() == CanvasParts::EdgeArrow)
                item = static_cast<CanvasCFGEdgeArrow*>(item)->canvasEdge();

            if (item->type() == CanvasParts::Edge)
            {
                #if 0
                branch = static_cast<CanvasCFGEdge*>(item)->edge()->branch();
                if (branch)
                {
                    actions[MenuActions::activateBranch] =
                            popup.addAction(QObject::tr("Go to \'%1\'")
                                            .arg(GlobalConfig::shortenSymbol(branch->prettyName())));
                    popup.addSeparator();
                }
                #endif
            }
        }
    }

    return bb;
}

QAction* addStopLayoutAction(QMenu& topLevel, QProcess* renderProcess)
{
    if (renderProcess)
    {
        QAction* stopLayout_ = topLevel.addAction(QObject::tr("Stop Layouting"));
        topLevel.addSeparator();

        return stopLayout_;
    }
    else
        return nullptr;
}

} // unnamed namespace

void ControlFlowGraphView::contextMenuEvent(QContextMenuEvent* event)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::contextMenuEvent" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    _isMoving = false;

    std::array<QAction*, MenuActions::nActions> actions;
    actions[MenuActions::activateBasicBlock] = nullptr;
    #if 0
    actions[MenuActions::activateBranch] = nullptr;
    #endif

    QMenu popup;
    TraceBasicBlock* bb = addNodesOrEdgesAction(popup, itemAt(event->pos()), actions);

    actions[MenuActions::stopLayout] = addStopLayoutAction(popup, _renderProcess);

    addGoMenu(std::addressof(popup));
    popup.addSeparator();

    auto exportMenu = popup.addMenu(QObject::tr("Export Graph"));
    actions[MenuActions::exportAsDot] = exportMenu->addAction(QObject::tr("As DOT file..."));
    actions[MenuActions::exportAsImage] = exportMenu->addAction(QObject::tr("As Image..."));
    popup.addSeparator();

    auto graphMenu = popup.addMenu(QObject::tr("Graph"));

    addPredecessorDepthMenu(graphMenu);
    addSuccessorDepthMenu(graphMenu);
    addNodeLimitMenu(graphMenu);
    addBranchLimitMenu(graphMenu);
    graphMenu->addSeparator();

    addLayoutMenu(std::addressof(popup));
    addZoomPosMenu(std::addressof(popup));

    auto action = popup.exec(event->globalPos());
    auto index = std::distance(actions.begin(),
                               std::find(actions.begin(), actions.end(), action));

    switch(index)
    {
        case MenuActions::activateBasicBlock:
            activated(bb);
            break;
        #if 0
        case MenuActions::activateBranch:
            activated(branch);
            break;
        #endif
        case MenuActions::stopLayout:
            stopRendering();
            break;
        case MenuActions::exportAsDot:
        {
            TraceFunction* func = activeFunction();
            if (func)
                CFGExporter::savePrompt(this, func, eventType(), groupType(), _exporter.layout());
            break;
        }
        case MenuActions::exportAsImage:
            if (_scene)
                exportGraphAsImage();
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

    auto fileName = QFileDialog::getSaveFileName(this,
                                                 QObject::tr("Export Graph as Image"),
                                                 QString{},
                                                 QObject::tr("Images (*.png *.jpg)"));
    if (!fileName.isEmpty())
    {
        auto rect = _scene->sceneRect().toRect();
        QPixmap pix{rect.width(), rect.height()};
        QPainter painter{std::addressof(pix)};
        _scene->render(std::addressof(painter));
        pix.save(fileName);
    }
}

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

        #if 0
        else if (_selectedEdge && _selectedEdge->branch())
            activated(_selectedEdge->branch());
        #endif
    }
    else if (!(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) &&
             (_selectedNode || _selectedEdge))
    {
        auto [node, edge] = getNodeAndEdgeToSelect(transformKeyIfNeeded(e->key()));

        if (node && node->basicBlock())
            selected(node->basicBlock());
        #if 0
        else if (edge && edge->branch())
            selected(edge->branch());
        #endif
    }
    else
        movePointOfView(e);
}

int ControlFlowGraphView::transformKeyIfNeeded(int key)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::transformKeyIfNeeded" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_exporter.layout() == CFGExporter::Layout::LeftRight)
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

std::pair<CFGNode*, CFGEdge*> ControlFlowGraphView::getNodeAndEdgeToSelect(int key)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::getNodeAndEdgeToSelect" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    CFGNode* node;
    CFGEdge* edge;

    if (_selectedNode)
    {
        node = nullptr;

        switch (key)
        {
            case Qt::Key_Up:
                edge = _selectedNode->visiblePredecessorEdge();
                break;
            case Qt::Key_Down:
                edge = _selectedNode->visibleSuccessorEdge();
                break;
            default:
                edge = nullptr;
                break;
        }
    }
    else if (_selectedEdge)
    {
        switch (key)
        {
            case Qt::Key_Up:
                node = _selectedEdge->cachedFromNode();
                edge = nullptr;
                break;
            case Qt::Key_Down:
                node = _selectedEdge->cachedToNode();
                edge = nullptr;
                break;
            case Qt::Key_Left:
                node = nullptr;
                edge = _selectedEdge->priorVisibleEdge();
                break;
            case Qt::Key_Right:
                node = nullptr;
                edge = _selectedEdge->nextVisibleEdge();
                break;
            default:
                node = nullptr;
                edge = nullptr;
                break;
        }
    }
    else
    {
        node = nullptr;
        edge = nullptr;
    }

    return std::pair{node, edge};
}

void ControlFlowGraphView::movePointOfView(QKeyEvent* e)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::movePointOfView" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    auto dx = [this]{ return mapToScene(width(), 0) - mapToScene(0, 0); };
    auto dy = [this]{ return mapToScene(0, height()) - mapToScene(0, 0); };

    auto center = mapToScene(viewport()->rect().center());
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
            auto delta = dy();
            centerOn(center + QPointF(-delta.x() / 2.0, -delta.y() / 2.0));
            break;
        }
        case Qt::Key_PageDown:
        {
            auto delta = dy();
            centerOn(center + QPointF(delta.x() / 2.0, delta.y() / 2.0));
            break;
        }
        case Qt::Key_Left:
        {
            auto delta = dx();
            centerOn(center + QPointF(-delta.x() / 10.0, -delta.y() / 10.0));
            break;
        }
        case Qt::Key_Right:
        {
            auto delta = dx();
            centerOn(center + QPointF(delta.x() / 10.0, delta.y() / 10.0));
            break;
        }
        case Qt::Key_Down:
        {
            auto delta = dy();
            centerOn(center + QPointF(delta.x() / 10.0, delta.y() / 10.0));
            break;
        }
        case Qt::Key_Up:
        {
            auto delta = dy();
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

    auto topLeft = mapToScene(QPoint(0, 0));
    auto bottomRight = mapToScene(QPoint(width(), height()));

    QRectF z{topLeft, bottomRight};
    _panningView->setZoomRect(z);
}

namespace
{

double calculate_zoom (QSize s, int cWidth, int cHeight)
{
    // first, assume use of 1/3 of width/height (possible larger)
    auto zoom = (s.width() * cHeight < s.height() * cWidth) ? .33 * s.height() / cHeight
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

    auto zoom = calculate_zoom (s, cWidth, cHeight);

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

    auto cvW = _panningView->width();
    auto cvH = _panningView->height();
    auto x = width() - cvW - verticalScrollBar()->width() - 2;
    auto y = height() - cvH - horizontalScrollBar()->height() - 2;

    auto zp = _zoomPosition;
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

    auto newZoomPos = QPoint{0, 0};
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

            #if 0
            case ProfileContext::Branch:
            {
                CFGEdge* edge = _exporter.findEdge(static_cast<TraceBranch*>(_selectedItem));
                if (edge == _selectedEdge)
                    return;

                unselectNode();
                unselectEdge();
                selectEdge(edge);
                break;
            }
            #endif

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

void ControlFlowGraphView::refresh()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::refresh()" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_renderProcess)
        stopRendering();

    _prevSelectedNode = _selectedNode;
    if (_selectedNode)
    {
        auto center = _selectedNode->canvasNode()->rect().center();
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
        case ProfileContext::Function:
        case ProfileContext::Call:
            break;
        default:
            showText(QObject::tr("No control-flow graph can be drawn for the active item."));
            return;
    }

    qDebug() << "ControlFlowGraphView::refresh";

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
    QStringList renderArgs{QStringLiteral("-Tplain")};

    _renderProcessCmdLine = renderProgram + QLatin1Char(' ') + renderArgs.join(QLatin1Char(' '));

    qDebug("ControlFlowGraphView::refresh: Starting process %p, \'%s\'",
           _renderProcess, qPrintable(_renderProcessCmdLine));

    auto process = _renderProcess;
    process->start(renderProgram, renderArgs);
    _exporter.reset(_activeItem, _eventType, _groupType);
    _exporter.writeDot(process);
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

QAction* ControlFlowGraphView::addPredecessorDepthAction(QMenu* m, QString s, int depth)
{
    QAction* a = m->addAction(s);

    a->setData(depth);
    a->setCheckable(true);
    a->setChecked(_maxCallerDepth == depth);

    return a;
}

QAction* ControlFlowGraphView::addSuccessorDepthAction(QMenu* m, QString s, int depth)
{
    QAction* a = m->addAction(s);

    a->setData(depth);
    a->setCheckable(true);
    a->setChecked(_maxCalleeDepth == depth);

    return a;
}

QAction* ControlFlowGraphView::addNodeLimitAction(QMenu* m, QString s, double limit)
{
    QAction* a = m->addAction(s);

    a->setData(limit);
    a->setCheckable(true);
    a->setChecked(_funcLimit == limit);

    return a;
}

QAction* ControlFlowGraphView::addBranchLimitAction(QMenu* m, QString s, double limit)
{
    QAction* a = m->addAction(s);

    a->setData(limit);
    a->setCheckable(true);
    a->setChecked(_callLimit == limit);

    return a;
}

QAction* ControlFlowGraphView::addZoomPosAction(QMenu* m, QString s, ControlFlowGraphView::ZoomPosition pos)
{
    QAction* a = m->addAction(s);

    a->setData(static_cast<int>(pos));
    a->setCheckable(true);
    a->setChecked(_zoomPosition == pos);

    return a;
}

QAction* ControlFlowGraphView::addLayoutAction(QMenu* m, QString s, CFGExporter::Layout layout)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addLayoutAction" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QAction* a = m->addAction(s);

    a->setData(static_cast<int>(layout));
    a->setCheckable(true);
    a->setChecked(_exporter.layout() == layout);

    return a;
}

QMenu* ControlFlowGraphView::addPredecessorDepthMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(QObject::tr("Predecessor Depth"));
    QAction* a = addPredecessorDepthAction(m, QObject::tr("Unlimited"), -1);

    a->setEnabled(_funcLimit > 0.005);
    m->addSeparator();

    addPredecessorDepthAction(m, QObject::tr("Depth 0", "None"), 0);
    addPredecessorDepthAction(m, QObject::tr("max. 1"), 1);
    addPredecessorDepthAction(m, QObject::tr("max. 2"), 2);
    addPredecessorDepthAction(m, QObject::tr("max. 5"), 5);
    addPredecessorDepthAction(m, QObject::tr("max. 10"), 10);
    addPredecessorDepthAction(m, QObject::tr("max. 15"), 15);

    #if 0
    connect(m, &QMenu::triggered,
            this, &ControlFlowGraphView::predecessorDepthTriggered);
    #endif

    return m;
}

QMenu* ControlFlowGraphView::addSuccessorDepthMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(QObject::tr("Successor Depth"));
    QAction* a = addSuccessorDepthAction(m, QObject::tr("Unlimited"), -1);

    a->setEnabled(_funcLimit > 0.005);
    m->addSeparator();

    addSuccessorDepthAction(m, QObject::tr("Depth 0", "None"), 0);
    addSuccessorDepthAction(m, QObject::tr("max. 1"), 1);
    addSuccessorDepthAction(m, QObject::tr("max. 2"), 2);
    addSuccessorDepthAction(m, QObject::tr("max. 5"), 5);
    addSuccessorDepthAction(m, QObject::tr("max. 10"), 10);
    addSuccessorDepthAction(m, QObject::tr("max. 15"), 15);

    #if 0
    connect(m, &QMenu::triggered,
            this, &ControlFlowGraphView::successorDepthTriggered);
    #endif

    return m;
}

QMenu* ControlFlowGraphView::addNodeLimitMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(QObject::tr("Min. Node Cost"));
    QAction* a = addNodeLimitAction(m, QObject::tr("No Minimum"), 0.0);

    a->setEnabled(_maxCallerDepth >= 0 && _maxCalleeDepth >= 0);
    m->addSeparator();

    addNodeLimitAction(m, QObject::tr("50 %"), 0.5);
    addNodeLimitAction(m, QObject::tr("20 %"), 0.2);
    addNodeLimitAction(m, QObject::tr("10 %"), 0.1);
    addNodeLimitAction(m, QObject::tr("5 %"), 0.05);
    addNodeLimitAction(m, QObject::tr("2 %"), 0.02);
    addNodeLimitAction(m, QObject::tr("1 %"), 0.01);

    #if 0
    connect(m, &QMenu::triggered,
            this, &ControlFlowGraphView::nodeLimitTriggered);
    #endif

    return m;
}

QMenu* ControlFlowGraphView::addBranchLimitMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(QObject::tr("Min. Branch Cost"));

    addBranchLimitAction(m, QObject::tr("Same as Node"), 1.0);
    addBranchLimitAction(m, QObject::tr("50 % of Node"), 0.5);
    addBranchLimitAction(m, QObject::tr("20 % of Node"), 0.2);
    addBranchLimitAction(m, QObject::tr("10 % of Node"), 0.1);

    #if 0
    connect(m, &QMenu::triggered,
            this, &ControlFlowGraphView::branchLimitTriggered);
    #endif

    return m;
}

QMenu* ControlFlowGraphView::addZoomPosMenu(QMenu* menu)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addZoomPosMenu" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QMenu* m = menu->addMenu(QObject::tr("Birds-eye View"));

    addZoomPosAction(m, QObject::tr("Top Left"), ZoomPosition::TopLeft);
    addZoomPosAction(m, QObject::tr("Top Right"), ZoomPosition::TopRight);
    addZoomPosAction(m, QObject::tr("Bottom Left"), ZoomPosition::BottomLeft);
    addZoomPosAction(m, QObject::tr("Bottom Right"), ZoomPosition::BottomRight);
    addZoomPosAction(m, QObject::tr("Automatic"), ZoomPosition::Auto);
    addZoomPosAction(m, QObject::tr("Hide"), ZoomPosition::Hide);

    connect(m, &QMenu::triggered,
            this, &ControlFlowGraphView::zoomPosTriggered);

    return m;
}

QMenu* ControlFlowGraphView::addLayoutMenu(QMenu* menu)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::addLayoutMenu" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QMenu* m = menu->addMenu(QObject::tr("Layout"));

    addLayoutAction(m, QObject::tr("Top to Down"), CFGExporter::Layout::TopDown);
    addLayoutAction(m, QObject::tr("Left to Right"), CFGExporter::Layout::LeftRight);

    connect(m, &QMenu::triggered,
            this, &ControlFlowGraphView::layoutTriggered);

    return m;
}

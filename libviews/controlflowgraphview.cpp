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

    auto edgeComp = [](const CFGEdge *ge1, const CFGEdge *ge2)
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
            auto &p1 = ce1->controlPoints();
            auto &p2 = ce2->controlPoints();
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
                            [](CFGEdge *e1, CFGEdge *e2){ return e1->cost < e2->cost; });
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
    _detailLevel = DetailLevel::avgDetails;
    _layout = Layout::TopDown;

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
                assert (!BBs.empty());
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

    for (auto &node : _nodeMap)
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
    QFile *file = nullptr;
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

    bool ok = true;
    if (!_graphCreated)
        ok = createGraph();

    // Following block is for debug purposes
    // ---------------------------------- //
    delete stream;
    QFile dotFile{"graph.dot"};
    dotFile.open(QFile::WriteOnly);
    stream = new QTextStream{&dotFile};
    // ---------------------------------- //

    if (ok)
    {
        *stream << "digraph \"control-flow graph\" {\n";

        dumpLayoutSettings(*stream);
        dumpNodes(*stream);
        dumpEdges(*stream);

        #if 0
        if (_go->showSkipped())
        {
            for (auto &n : _nodeMap)
            {
                if (n.incl <= _realBBLimit)
                    continue;

                dumpSkippedPredecessor(ts, n);
                dumpSkippedSuccessor(ts, n);
            }
        }
        #endif

        for (auto &n : _nodeMap)
            n.clearEdges();

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
            assert (!BBs.empty());
            _item = BBs.front();
            break;
        }
        case ProfileContext::Call:
        {
            auto f = static_cast<TraceCall*>(_item)->caller(false);
            auto& BBs = f->basicBlocks();
            assert (!BBs.empty());
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

CFGNode *CFGExporter::buildNode(TraceBasicBlock* bb)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::buildNode()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    assert(bb);

    auto &node = _nodeMap[std::make_pair(bb->firstAddr(), bb->lastAddr())];
    auto nodePtr = std::addressof(node);

    if (!node.basicBlock())
    {
        node.setBasicBlock(bb);
        buildEdge(nodePtr, std::addressof(bb->trueBranch()));
        buildEdge(nodePtr, std::addressof(bb->falseBranch()));
    }

    return nodePtr;
}

void CFGExporter::buildEdge(CFGNode* fromNode, TraceBranch* branch)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::buildEdge()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    assert(fromNode);

    TraceBasicBlock* to = branch->toBB();
    if (to)
    {
        TraceBasicBlock* from = branch->fromBB();
        auto &edge = _edgeMap[std::make_pair(from, to)];

        edge.setBranch(branch);

        if (!edge.fromNode())
            edge.setPredecessorNode(fromNode);

        if (!edge.toNode())
        {
            if (to == from)
                edge.setSuccessorNode(fromNode);
            else
                edge.setSuccessorNode(buildNode(to));
        }
    }
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

    QMap<Addr, QString> getInstrStrings();

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
    assert (!instrMap->empty());

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

        auto args = QStringLiteral(" -C -d --start-address=0x%1 --stop-address=0x%2 %3")
                                  .arg(_dumpStartAddr.toString())
                                  .arg((_dumpEndAddr + 20).toString())
                                  .arg(_objFile);

        _objdumpCmd = objdumpFormat + args;

        qDebug("Running \'%s\'...", qPrintable(_objdumpCmd));

        _objdump.start(_objdumpCmd);
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

QMap<Addr, QString> ObjdumpParser::getInstrStrings()
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
            assert (addr == _objAddr);

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
                assert (!encoding.isNull());

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
        qDebug() << QObject::tr("There are %n cost line(s) without machine code.", "", _noAssLines)
                 << QObject::tr("This happens because the code of")
                 << QStringLiteral("    %1").arg(_objFile)
                 << QObject::tr("does not seem to match the profile data file.")
                 << QObject::tr("Are you using an old profile data file or is the above mentioned")
                 << QObject::tr("ELF object from an updated installation/another machine?");

        return {};
    }
    else if (instrStrings.empty())
    {
        qDebug() << QObject::tr("There seems to be an error trying to execute the command")
                 << QStringLiteral("    '%1'").arg(_objdumpCmd)
                 << QObject::tr("Check that the ELF object used in the command exists.")
                 << QObject::tr("Check that you have installed 'objdump'.")
                 << QObject::tr("This utility can be found in the 'binutils' package.");

        return {};
    }

    return instrStrings;
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
            if (readBytes == _line.capacity())
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

bool CFGExporter::fillInstrStrings(TraceFunction *func)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::fillInstrStrings()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    assert(func);

    if (_nodeMap.empty())
        return false;

    ObjdumpParser parser{func};
    auto instrStrings = parser.getInstrStrings();
    if (instrStrings.empty())
        return false;

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

void CFGExporter::dumpLayoutSettings(QTextStream &ts)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::dumpLayoutSettings()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (_layout == Layout::LeftRight)
        ts << QStringLiteral("  rankdir=LR;\n");
    else if (_layout == Layout::Circular)
    {
        TraceBasicBlock *bb;
        switch (_item->type())
        {
            case ProfileContext::Function:
            case ProfileContext::FunctionCycle:
            {
                auto& BBs = static_cast<TraceFunction*>(_item)->basicBlocks();
                assert (!BBs.empty());
                bb = BBs.front();
                break;
            }
            case ProfileContext::Call:
            {
                auto f = static_cast<TraceCall*>(_item)->caller(true);
                auto& BBs = f->basicBlocks();
                assert(!BBs.empty());
                bb = BBs.front();
                break;
            }
            case ProfileContext::BasicBlock:
                bb = static_cast<TraceBasicBlock*>(_item);
                break;
            default:
                bb = nullptr;
        }

        if (bb)
            ts << QStringLiteral("  center=B%1;\n")
                                .arg(reinterpret_cast<std::ptrdiff_t>(bb), 0, 16);

        ts << QStringLiteral("  overlap=false\n  splines=true;\n");
    }
}

void CFGExporter::dumpNodes(QTextStream &ts)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::dumpNodes()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    for (auto& node : _nodeMap)
    {
        TraceBasicBlock* bb = node.basicBlock();

        ts << QStringLiteral("  B%1 [shape=record, label=\"{")
                            .arg(bb->firstAddr().toString());

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
           << *std::prev(node.end()) << "}\"]\n";
    }
}

void CFGExporter::dumpEdges(QTextStream &ts)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::dumpEdges()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    for (auto &edge : _edgeMap)
    {
        TraceBranch* br = edge.branch();
        auto portFrom = br->fromBB()->lastInstr()->addr().toString();

        switch (br->type())
        {
            case TraceBranch::Type::true_:
            case TraceBranch::Type::unconditional:
            {
                const char *color = (br->type() == TraceBranch::Type::true_) ? "blue" : "black";

                ts << QStringLiteral("  B%1:I%2:w -> B%3")
                                    .arg(br->fromBB()->firstAddr().toString())
                                    .arg(portFrom)
                                    .arg(br->toBB()->firstAddr().toString());

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
                ts << QStringLiteral("  B%1:I%2:e -> B%3:n [color=red]\n")
                                    .arg(br->fromBB()->firstAddr().toString())
                                    .arg(portFrom)
                                    .arg(br->toBB()->firstAddr().toString());
                break;

            default:
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

TraceBasicBlock* CFGExporter::toBasicBlock(QString s)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::toBasicBlock()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    if (s[0] != 'B')
        return nullptr;

    bool ok;
    auto bb = reinterpret_cast<TraceBasicBlock*>(s.mid(1).toULongLong(&ok, 16));

    return ok ? bb : nullptr;
}

bool CFGExporter::savePrompt(QWidget *parent, TraceFunction *func,
                             EventType *eventType, ProfileContext::Type groupType)
{
    #ifdef CFGEXPORTER_DEBUG
    qDebug() << "\033[1;31m" << "CFGExporter::savePrompt()" << "\033[0m";
    #endif // CFGEXPORTER_DEBUG

    static constexpr const char *filter1 = "text/vnd.graphviz";
    static constexpr const char *filter2 = "application/pdf";
    static constexpr const char *filter3 = "application/postscript";

    QFileDialog saveDialog{parent, QObject::tr("Export Graph")};
    saveDialog.setMimeTypeFilters(QStringList{filter1, filter2, filter3});
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);

    if (saveDialog.exec())
    {
        auto &intendedName = saveDialog.selectedFiles().first();
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
            dotRenderType = "-Tpdf";
        }
        else if (mime == filter2)
        {
            dotName = maybeTemp.fileName();
            dotRenderType = "-Tps";
        }
        else if (mime == filter3)
        {
            dotName = maybeTemp.fileName();
            dotRenderType = "-Tps";
        }

        CFGExporter ge{func, eventType, groupType, dotName};

        bool wrote = ge.writeDot();
        if (wrote && mime != filter1)
        {
            QProcess proc;
            proc.setStandardOutputFile(intendedName, QFile::Truncate);
            proc.start("dot", QStringList{ dotRenderType, dotName },
                       QProcess::ReadWrite);
            proc.waitForFinished();
            wrote = (proc.exitStatus() == QProcess::NormalExit);

            if (wrote)
                QDesktopServices::openUrl(QUrl::fromLocalFile(intendedName));
        }

        return wrote;
    }

    return false;
}

#if 0
void CFGExporter::dumpSkippedPredecessor(QTextStream &ts, const CFGNode &n)
{
    auto costSum = n.predecessorCostSum();

    if (costSum > _realBranchLimit)
    {
        auto bb = n.basicBlock();
        std::pair<TraceBasicBlock*, TraceBasicBlock*> p{nullptr, bb};
        auto &edge = _edgeMap[p];

        edge.setSuccessor(bb);
        edge.cost = costSum;
        edge.count = n.predecessorCountSum();

        ts << QStringLiteral("  R%1 [shape=point,label=\"\"];\n")
                            .arg(reinterpret_cast<std::ptrdiff_t>(bb), 0, 16);
        ts << QStringLiteral("  R%1 -> B%2 [label=\"%3\\n%4 x\",weight=%5];\n")
                            .arg(reinterpret_cast<std::ptrdiff_t>(bb), 0, 16)
                            .arg(reinterpret_cast<std::ptrdiff_t>(bb), 0, 16)
                            .arg(SubCost{costSum}.pretty())
                            .arg(SubCost{edge.count}.pretty())
                            .arg(static_cast<int>(std::log(costSum)));
    }
}

void CFGExporter::dumpSkippedSuccessor(QTextStream &ts, const CFGNode &n)
{
    auto costSum = n.successorCostSum();

    if (costSum > _realBranchLimit)
    {
        auto bb = n.basicBlock();
        std::pair<TraceBasicBlock*, TraceBasicBlock*> p{bb, nullptr};
        auto &edge = _edgeMap[p];

        edge.setPredecessor(bb);
        edge.cost = costSum;
        edge.count = n.successorCountSum();

        ts << QStringLiteral("  S%1 [shape=point,label=\"\"];\n")
                            .arg(reinterpret_cast<std::ptrdiff_t>(bb), 0, 16);
        ts << QStringLiteral("  B%1 -> S%2 [label=\"%3\\n%4 x\",weight=%5];\n")
                            .arg(reinterpret_cast<std::ptrdiff_t>(bb), 0, 16)
                            .arg(reinterpret_cast<std::ptrdiff_t>(bb), 0, 16)
                            .arg(SubCost{costSum}.pretty())
                            .arg(SubCost{edge.count}.pretty())
                            .arg(static_cast<int>(std::log(costSum)));
    }
}
#endif

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
    if (!_node || !_view)
        return;

    updateGroup(); // set node's color

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

void CanvasCFGNode::updateGroup()
{
    if (!_node || !_view)
        return;

    auto color = GlobalGUIConfig::basicBlockColor(_view->groupType(), _node->basicBlock());
    setBackColor(color);

    update();
}

void CanvasCFGNode::setSelected(bool s)
{
    StoredDrawParams::setSelected(s);
    update();
}

void CanvasCFGNode::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    auto nInstructions = _node->instrNumber();
    p->setPen(Qt::black);

    QRect rectangle = rect().toRect();
    QRect bRect = p->boundingRect(rectangle.x(), rectangle.y(), rectangle.width(),
                                  rectangle.height() / nInstructions,
                                  Qt::AlignCenter, _node->longestInstr());

    int step = bRect.height() + 5;
    rectangle.setWidth(bRect.width() + 5);
    rectangle.setHeight(step * nInstructions);

    p->fillRect(rectangle.x(), rectangle.y() + rectangle.height() - step,
                rectangle.width(), step, Qt::gray);

    p->drawRect(rectangle);
    p->drawText(rectangle.x(), rectangle.y(), rectangle.width(), step,
                Qt::AlignCenter, *_node->begin());

    auto i = 0;
    for (auto it = std::next(_node->begin()), ite = _node->end(); it != ite; ++it, ++i)
    {
        auto bottomRightCorner = rectangle.y() + step * i;
        p->drawText(rectangle.x(), bottomRightCorner,
                    rectangle.width(), step,
                    Qt::AlignCenter, *it);
        p->drawLine(rectangle.x(), bottomRightCorner,
                    rectangle.x() + rectangle.width(), bottomRightCorner);
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

CanvasCFGEdgeArrow::CanvasCFGEdgeArrow(CanvasCFGEdge* ce) : _ce{ce} {}

void CanvasCFGEdgeArrow::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(_ce->isSelected() ? Qt::red : Qt::black);
    p->drawPolygon(polygon(), Qt::OddEvenFill);
}

// ======================================================================================

//
// CanvasCFGEdge
//

CanvasCFGEdge::CanvasCFGEdge(CFGEdge *e) : _edge{e}
{
    setFlag(QGraphicsItem::ItemIsSelectable);
}

void CanvasCFGEdge::setLabel(CanvasCFGEdgeLabel* l)
{
    _label = l;

    if (_label)
    {
        auto tip = QStringLiteral("%1 (%2)").arg(l->text(0)).arg(l->text(1));

        setToolTip(tip);
        if (_arrow)
            _arrow->setToolTip(tip);

        _thickness = std::max(0.9, std::log(l->percentage()));
    }
}

void CanvasCFGEdge::setArrow(CanvasCFGEdgeArrow* a)
{
    _arrow = a;

    if (_arrow && _label)
        a->setToolTip(QStringLiteral("%1 (%2)")
                                    .arg(_label->text(0)).arg(_label->text(1)));
}

void CanvasCFGEdge::setControlPoints(const QPolygon &p)
{
    _points = p;

    QPainterPath path;
    path.moveTo(p[0]);
    for (auto i = 1; i < p.size(); i += 3)
        path.cubicTo(p[i], p[(i + 1) % p.size()], p[(i + 2) % p.size()]);

    setPath(path);
}

void CanvasCFGEdge::setSelected(bool s)
{
    QGraphicsItem::setSelected(s);
    update();
}

void CanvasCFGEdge::paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget*)
{
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
// CanvasCFGFrame
//

QPixmap* CanvasCFGFrame::_p = nullptr;

CanvasCFGFrame::CanvasCFGFrame(CanvasCFGNode* node)
{
    if (!_p)
    {
        QRect rect{-435, -435, 900, 900};

        _p = new QPixmap(rect.size());
        _p->fill(Qt::white);
        QPainter p{_p};
        p.setPen(Qt::NoPen);

        rect.translate(-rect.x(), -rect.y());

        constexpr auto nStages = 87;
        constexpr auto d = 5;
        auto v1 = 130.0f;
        auto f = 1.03f;
        auto v = v1 / (std::pow (f, nStages));
        while (v < v1)
        {
            v *= f;

            p.setBrush(QColor{265 - static_cast<int>(v),
                              265 - static_cast<int>(v),
                              265 - static_cast<int>(v)});
            p.drawRect(QRect(rect.x(), rect.y(), rect.width(), d));
            p.drawRect(QRect(rect.x(), rect.bottom() - d, rect.width(), d));
            p.drawRect(QRect(rect.x(), rect.y() + d, d, rect.height() - 2*d));

            rect.setRect(rect.x() + d,         rect.y() + d,
                         rect.width() - 2 * d, rect.height() - 2 * d);
        }
    }

    setRect(node->rect().center().x() - _p->width() / 2,
            node->rect().center().y() - _p->height() / 2,
            _p->width(), _p->height());
}

void CanvasCFGFrame::paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget*)
{
#if QT_VERSION >= 0x040600
    auto levelOfDetail = option->levelOfDetailFromTransform(p->transform());
#else
    auto levelOfDetail = option->levelOfDetail;
#endif

    if (levelOfDetail < 0.5)
    {
        QRadialGradient g{rect().center(), rect().width() / 3};
        g.setColorAt(0.0, Qt::gray);
        g.setColorAt(1.0, Qt::white);

        p->setBrush(QBrush(g));
        p->setPen(Qt::NoPen);
        p->drawRect(rect());
    }
    else
        p->drawPixmap(static_cast<int>(rect().x()), static_cast<int>(rect().y()), *_p);
}

// ======================================================================================

//
// ControlFlowGraphView
//

ControlFlowGraphView::ControlFlowGraphView(TraceItemView* parentView, QWidget* parent,
                                           const QString &name) :
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

void ControlFlowGraphView::showRenderError(const QString &text)
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
    auto [scaleX, scaleY] = calculateScales(dotStream);

    QString cmd;
    double dotHeight = 0.0;
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
                dotHeight = setupScreen(lineStream, scaleX, scaleY, lineno);
            else
            {
                bool isNode = (cmd == QLatin1String("node"));
                bool isEdge = (cmd == QLatin1String("edge"));

                if (!isNode && !isEdge)
                    qDebug() << "Ignoring unknown command \'" << cmd << "\' from dot ("
                            << _exporter.filename() << ":" << lineno << ")";
                else if (!_scene)
                    qDebug() << "Ignoring \'" << cmd << "\' without \'graph\' form dot ("
                            << _exporter.filename() << ":" << lineno << ")";
                else if (isNode)
                    activeNode = parseNode(activeNode, lineStream, scaleX, scaleY, dotHeight);
                else // if (isEdge)
                    activeEdge = parseEdge(activeEdge, lineStream, scaleX, scaleY, dotHeight,
                                           lineno);
            }
        }
    }

    return std::pair{activeNode, activeEdge};
}

namespace
{

double getMaxNodeHeight(QTextStream &ts)
{
    #ifdef DEBUG
    qDebug() << "\033[1;31m" << "namespace::getNodeHeight" << "\033[0m";
    #endif // DEBUG

    QString cmd;

    double maxNodeHeight = 0.0;
    while (true)
    {
        QString line = ts.readLine();

        if (line.isNull())
            break;
        else if (!line.isEmpty())
        {
            QTextStream lineStream{std::addressof(line), QIODevice::ReadOnly};
            lineStream >> cmd;

            if (cmd == QLatin1String("node"))
            {
                QString h; // first 4 values are overriden
                lineStream >> h /* name */ >> h /* x */ >> h /* y */
                                           >> h /* width */ >> h /* height */;
                maxNodeHeight = std::max(maxNodeHeight, h.toDouble());
            }
        }
    }
    ts.seek(0);

    return maxNodeHeight;
}

} // unnamed namespace

std::pair<double, double> ControlFlowGraphView::calculateScales(QTextStream &ts)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::calculateScales" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    auto maxNodeHeight = getMaxNodeHeight(ts);

    double scaleX, scaleY;
    if (maxNodeHeight > 0.0)
    {
        scaleX = 80.0;
        scaleY = (8 + (1 + 2 * _exporter.detailLevel()) * fontMetrics().height()) / maxNodeHeight;
    }
    else
        scaleX = scaleY = 1.0;

    return std::pair{scaleX, scaleY};
}

double ControlFlowGraphView::setupScreen(QTextStream &ts, double scaleX, double scaleY, int lineno)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::setupScreen" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    double dotHeight;
    QString dotWidthString, dotHeightString;
    ts >> dotHeight >> dotWidthString >> dotHeightString;

    dotHeight = dotHeightString.toDouble(); // overrides previous unused value

    if (_scene)
        qDebug() << "Ignoring 2nd \'graph\' from dot ("
                 << _exporter.filename() << ":" << lineno << ")";
    else
    {
        QSize pScreenSize = QApplication::primaryScreen()->size();

        _xMargin = 50;
        auto w = static_cast<int>(scaleX * dotWidthString.toDouble());
        if (w < pScreenSize.width())
            _xMargin += (pScreenSize.width() - w) / 2;

        _yMargin = 50;
        auto h = static_cast<int>(scaleY * dotHeight);
        if (h < pScreenSize.height())
            _yMargin += (pScreenSize.height() - h) / 2;

        _scene = new QGraphicsScene{0.0, 0.0, static_cast<qreal>(w + 2 * _xMargin),
                                              static_cast<qreal>(h + 2 * _yMargin)};
        _scene->setBackgroundBrush(Qt::white);
    }

    return dotHeight;
}

std::pair<int, int> ControlFlowGraphView::calculateSizes(QTextStream &ts, double scaleX,
                                                                          double scaleY,
                                                                          double dotHeight)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::calculateSizes" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString xStr, yStr;
    ts >> xStr >> yStr;

    auto xx = static_cast<int>(scaleX * xStr.toDouble() + _xMargin);
    auto yy = static_cast<int>(scaleY * (dotHeight - yStr.toDouble()) + _yMargin);

    return std::pair{xx, yy};
}

CFGNode* ControlFlowGraphView::parseNode(CFGNode* activeNode, QTextStream &ts, double scaleX,
                                         double scaleY, double dotHeight)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::parseNode" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString nodeName;
    ts >> nodeName;

    auto [xx, yy] = calculateSizes(ts, scaleX, scaleY, dotHeight);

    QString nodeWidth, nodeHeight;
    ts >> nodeWidth >> nodeHeight;

    if (nodeName[0] == 'R' || nodeName[0] == 'S')
    {
        auto eItem = new QGraphicsEllipseItem{xx - 5.0, yy - 5.0, 10.0, 10.0};

        _scene->addItem(eItem);
        eItem->setBrush(Qt::gray);
        eItem->setZValue(1.0);
        eItem->show();
    }
    else
    {
        // THIS IS UB: nodeName no longer represents TraceBasicBlock* but Addr
        CFGNode* node = _exporter.findNode(_exporter.toBasicBlock(nodeName));
        if (node)
        {
            assert(node->instrNumber() > 0);
            node->setVisible(true);

            auto w = static_cast<int>(scaleX * nodeWidth.toDouble());
            auto h = static_cast<int>(scaleY * nodeHeight.toDouble());

            auto rItem = new CanvasCFGNode{this, node, static_cast<qreal>(xx - w / 2),
                                                       static_cast<qreal>(yy - h / 2),
                                                       static_cast<qreal>(w),
                                                       static_cast<qreal>(h)};
            #if 0
            if (_detailLevel > 0)
                rItem->setMaxLines(0, 2 * _detailLevel);
            #endif

            _scene->addItem(rItem);
            node->setCanvasNode(rItem);

            if (node->basicBlock() == activeItem())
                activeNode = node;

            if (node->basicBlock() == selectedItem())
            {
                _selectedNode = node;
                rItem->setSelected(true);
            }
            else
                rItem->setSelected(node == _selectedNode);

            rItem->setZValue(1.0);
            rItem->show();
        }
        else
            qDebug("Warning: Unknown basic block \'%s\' ?!", qPrintable(nodeName));
    }

    return activeNode;
}

CFGEdge* ControlFlowGraphView::parseEdge(CFGEdge* activeEdge, QTextStream &ts, double scaleX,
                                         double scaleY, double dotHeight, int lineno)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::parseEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    QString node1Name, node2Name;
    ts >> node1Name >> node2Name;

    int nPoints;
    ts >> nPoints;

    // THIS IS UB: node1Name and node2Name no longer represent TraceBasicBlock* but Addr
    CFGEdge* edge = _exporter.findEdge(_exporter.toBasicBlock(node1Name),
                                       _exporter.toBasicBlock(node2Name));
    if (!edge)
    {
        qDebug() << "Unknown edge \'" << node1Name << "\'-\'" << node2Name << "\' from dot ("
                << _exporter.filename() << ":" << lineno << ")";

        return activeEdge;
    }

    edge->setVisible(true);

    QPolygon poly(nPoints);

    for (auto i = 0; i != nPoints; ++i)
    {
        if (ts.atEnd())
        {
            qDebug("ControlFlowGraphView: Can not read %d spline nPoints (%s:%d)",
                    nPoints, qPrintable(_exporter.filename()), lineno);
            return nullptr;
        }

        auto [xx, yy] = calculateSizes(ts, scaleX, scaleY, dotHeight);
        poly.setPoint(i, xx, yy);
    }

    [[maybe_unused]] CFGNode* predecessor = edge->fromNode();
    [[maybe_unused]] CFGNode* successor = edge->toNode();

    QColor arrowColor = Qt::black;

    auto sItem = new CanvasCFGEdge{edge};
    _scene->addItem(sItem);
    edge->setCanvasEdge(sItem);
    sItem->setControlPoints(poly);
    sItem->setPen(QPen{arrowColor});
    sItem->setZValue(0.5);
    sItem->show();

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

    QPoint arrowDir;
    int indexHead = -1;

    CanvasCFGNode* fromNode = edge->fromNode() ? edge->fromNode()->canvasNode() : nullptr;
    if (fromNode)
    {
        QPointF toCenter = fromNode->rect().center();
        qreal dx0 = poly.point(0).x() - toCenter.x();
        qreal dy0 = poly.point(0).y() - toCenter.y();
        qreal dx1 = poly.point(nPoints - 1).x() - toCenter.x();
        qreal dy1 = poly.point(nPoints - 1).y() - toCenter.y();

        if (dx0*dx0 + dy0*dy0 > dx1*dx1 + dy1*dy1)
            for (indexHead = 0; arrowDir.isNull() && indexHead != nPoints - 1; ++indexHead)
                arrowDir = poly.point(indexHead) - poly.point(indexHead + 1);
    }

    if (arrowDir.isNull())
        for (indexHead = nPoints - 1; arrowDir.isNull() && indexHead != 0; --indexHead)
            arrowDir = poly.point(indexHead) - poly.point(indexHead - 1);

    if (!arrowDir.isNull())
    {
        auto length = static_cast<double>(arrowDir.x() * arrowDir.x() +
                                          arrowDir.y() * arrowDir.y());
        arrowDir *= 10.0 / std::sqrt(length);

        auto headPoint = poly.point(indexHead);

        QPolygonF a;
        a << QPointF{headPoint + arrowDir};
        a << QPointF{headPoint + QPoint{arrowDir.y() / 2, -arrowDir.x() / 2}};
        a << QPointF{headPoint + QPoint{-arrowDir.y() / 2, arrowDir.x() / 2}};

        auto aItem = new CanvasCFGEdgeArrow(sItem);
        _scene->addItem(aItem);
        aItem->setPolygon(a);
        aItem->setBrush(arrowColor);
        aItem->setZValue(1.5);
        aItem->show();

        sItem->setArrow(aItem);
    }

    if (!ts.atEnd())
    {
        ts.skipWhiteSpace();

        QChar c;
        ts >> c;

        QString label;
        if (c == '\"')
        {
            ts >> c;
            while (!c.isNull() && c != '\"')
            {
                label.append(c);
                ts >> c;
            }
        }
        else
        {
            ts >> label;
            label.prepend(c);
        }

        auto [xx, yy] = calculateSizes(ts, scaleX, scaleY, dotHeight);
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

        auto message = QObject::tr("Error running the graph layouting tool.\n"
                                   "Please check that \'dot\' is installed (package Graphviz).");
        _scene->addSimpleText(message);
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

            auto frame = new CanvasCFGFrame(cn);
            _scene->addItem(frame);
            frame->setPos(cn->pos());
            frame->setZValue(-1);
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

void ControlFlowGraphView::successorDepthTriggered(QAction *a)
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

    _zoomPosition = static_cast<ZoomPosition>(a->data().toInt(nullptr));
    updateSizes();
}

void ControlFlowGraphView::layoutTriggered(QAction* a)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::layoutTriggered" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    _exporter.setLayout(static_cast<CFGExporter::Layout>(a->data().toInt(nullptr)));
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
        (this->*func)(node->basicBlock()->function());
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

    QGraphicsItem *item = itemAt(event->pos());
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
    toggleSkipped,
    toggleExpand,
    toggleCluster,
    layoutCompact,
    layoutNormal,
    layoutTall,

    // special value
    nActions
};

TraceBasicBlock* addNodesOrEdgesAction(QMenu &popup, QGraphicsItem *item,
                           std::array<QAction*, MenuActions::nActions> &actions)
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

QAction* addStopLayoutAction(QMenu &topLevel, QProcess* renderProcess)
{
    QAction* stopLayout;
    if (renderProcess)
    {
        stopLayout = topLevel.addAction(QObject::tr("Stop Layouting"));
        topLevel.addSeparator();
    }
    else
        stopLayout = nullptr;

    return stopLayout;
}

QAction* addToggleSkippedAction(QMenu *graphMenu, bool showSkipped)
{
    auto toggleSkipped_ = graphMenu->addAction(QObject::tr("Arrows for Skipped Calls"));
    toggleSkipped_->setCheckable(true);
    toggleSkipped_->setChecked(showSkipped);

    return toggleSkipped_;
}

QAction* addToggleExpandAction(QMenu *graphMenu, bool expandCycles)
{
    auto toggleExpand_ = graphMenu->addAction(QObject::tr("Inner-cycle Calls"));
    toggleExpand_->setCheckable(true);
    toggleExpand_->setChecked(expandCycles);

    return toggleExpand_;
}

QAction* addToggleClusterAction(QMenu *graphMenu, bool clusterGroups)
{
    auto toggleCluster_ = graphMenu->addAction(QObject::tr("Cluster Groups"));
    toggleCluster_->setCheckable(true);
    toggleCluster_->setChecked(clusterGroups);

    return toggleCluster_;
}

QAction* addLayoutCompactAction(QMenu *visualizationMenu, int detailLevel)
{
    auto layoutCompact_ = visualizationMenu->addAction(QObject::tr("Compact"));
    layoutCompact_->setCheckable(true);
    layoutCompact_->setChecked(detailLevel == 0);

    return layoutCompact_;
}

QAction* addLayoutNormalAction(QMenu *visualizationMenu, int detailLevel)
{
    auto layoutNormal_ = visualizationMenu->addAction(QObject::tr("Normal"));
    layoutNormal_->setCheckable(true);
    layoutNormal_->setChecked(detailLevel == 1);

    return layoutNormal_;
}

QAction* addLayoutTallAction(QMenu *visualizationMenu, int detailLevel)
{
    auto layoutTall_ = visualizationMenu->addAction(QObject::tr("Tall"));
    layoutTall_->setCheckable(true);
    layoutTall_->setChecked(detailLevel == 2);

    return layoutTall_;
}

void exportGraphAsImage(ControlFlowGraphView* view, QGraphicsScene* scene)
{
    assert(scene);

    auto fileName = QFileDialog::getSaveFileName(view,
                                                 QObject::tr("Export Graph as Image"),
                                                 QString{},
                                                 QObject::tr("Images (*.png *.jpg)"));
    if (!fileName.isEmpty())
    {
        auto rect = scene->sceneRect().toRect();
        QPixmap pix{rect.width(), rect.height()};
        QPainter painter{std::addressof(pix)};
        scene->render(std::addressof(painter));
        pix.save(fileName);
    }
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

    actions[MenuActions::toggleSkipped] = addToggleSkippedAction(graphMenu, _showSkipped);
    actions[MenuActions::toggleExpand] = addToggleExpandAction(graphMenu, _expandCycles);
    actions[MenuActions::toggleCluster] = addToggleClusterAction(graphMenu, _clusterGroups);

    auto visualizationMenu = popup.addMenu(QObject::tr("Visualization"));
    actions[MenuActions::layoutCompact] = addLayoutCompactAction(visualizationMenu, _detailLevel);
    actions[MenuActions::layoutNormal] = addLayoutNormalAction(visualizationMenu, _detailLevel);
    actions[MenuActions::layoutTall] = addLayoutTallAction(visualizationMenu, _detailLevel);

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
                CFGExporter::savePrompt(this, func, eventType(), groupType());
            break;
        }
        case MenuActions::exportAsImage:
            if (_scene)
                exportGraphAsImage(this, _scene);
            break;
        case MenuActions::toggleSkipped:
            _showSkipped ^= true;
            refresh();
            break;
        case MenuActions::toggleExpand:
            _expandCycles ^= true;
            refresh();
            break;
        case MenuActions::toggleCluster:
            _clusterGroups ^= true;
            refresh();
            break;
        case MenuActions::layoutCompact:
            _detailLevel = 0;
            refresh();
            break;
        case MenuActions::layoutNormal:
            _detailLevel = 1;
            refresh();
            break;
        case MenuActions::layoutTall:
            _detailLevel = 2;
            refresh();
            break;
        default: // practically nActions
            break;
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

        if (node->basicBlock())
            selected(node->basicBlock());
        #if 0
        else if (edge->branch())
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

    if (changeType == TraceItemView::eventType2Changed)
        return;
    else if (changeType == TraceItemView::selectedItemChanged)
    {
        if (!_scene || !_selectedItem)
            return;

        CFGNode* node;
        CFGEdge* edge;

        switch(_selectedItem->type())
        {
            case ProfileContext::BasicBlock:
                node = _exporter.findNode(static_cast<TraceBasicBlock*>(_selectedItem));
                if (node == _selectedNode)
                    return;
                edge = nullptr;
                break;
            case ProfileContext::Function:
            {
                auto func = static_cast<TraceFunction*>(_selectedItem);
                auto& BBs = func->basicBlocks();
                assert(!BBs.empty());
                TraceBasicBlock* bb = BBs.front();
                node = _exporter.findNode(bb);
                if (node == _selectedNode)
                    return;
                edge = nullptr;
                break;
            }

            // Function cycles are ignored because there are no instructions in them
            default:
                node = nullptr;
                edge = nullptr;
        }

        resetOldSelectedNodeAndEdge();
        setNewSelectedNodeAndEdge(node, edge);

        _scene->update();
        return;
    }
    else if (changeType == TraceItemView::groupTypeChanged)
    {
        if (!_scene)
            return;
        else if (!_clusterGroups)
        {
            for (auto item : _scene->items())
                if (item->type() == CanvasParts::Node)
                    static_cast<CanvasCFGNode*>(item)->updateGroup();

            _scene->update();
            return;
        }
    }
    else if (changeType & TraceItemView::dataChanged)
    {
        _exporter.reset(_activeItem, _eventType, _groupType);
        _selectedNode = nullptr;
        _selectedEdge = nullptr;
    }

    refresh();
}

void ControlFlowGraphView::resetOldSelectedNodeAndEdge()
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::resetOldSelectedNodeAndEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (_selectedNode && _selectedNode->canvasNode())
        _selectedNode->canvasNode()->setSelected(false);
    else if (_selectedNode && !_selectedNode->canvasNode())
        qDebug() << "\033[1;31m" << "No canvas node" << "\033[0m";
    _selectedNode = nullptr;

    if (_selectedEdge && _selectedEdge->canvasEdge())
        _selectedEdge->canvasEdge()->setSelected(false);
    _selectedEdge = nullptr;
}

void ControlFlowGraphView::setNewSelectedNodeAndEdge(CFGNode* node, CFGEdge* edge)
{
    #ifdef CONTROLFLOWGRAPHVIEW_DEBUG
    qDebug() << "\033[1;31m" << "ControlFlowGraphView::setNewSelectedNodeAndEdge" << "\033[0m";
    #endif // CONTROLFLOWGRAPHVIEW_DEBUG

    if (node && node->canvasNode())
    {
        _selectedNode = node;
        _selectedNode->canvasNode()->setSelected(true);

        if (!_isMoving)
        {
            auto cNode = _selectedNode->canvasNode();
            if (cNode)
                ensureVisible(cNode);
        }
    }

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
        case ProfileContext::FunctionCycle:
        case ProfileContext::Call:
            break;
        default:
            showText(QObject::tr("No control-flow graph can be drawn for the active item."));
            return;
    }

    qDebug() << "ControlFlowGraphView::refresh";

    _selectedNode = nullptr;
    _selectedEdge = nullptr;

    _unparsedOutput.clear();

    _renderTimer.setSingleShot(true);
    _renderTimer.start(1000);

    _renderProcess = new QProcess(this);

    connect(_renderProcess, &QProcess::readyReadStandardOutput,
            this, &ControlFlowGraphView::readDotOutput);

    auto errorPtr = static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error);
    connect(_renderProcess, errorPtr,
            this, &ControlFlowGraphView::dotError);

    auto finishedPtr = static_cast<void (QProcess::*)(int,
                                                      QProcess::ExitStatus)>(&QProcess::finished);
    connect(_renderProcess, finishedPtr,
            this, &ControlFlowGraphView::dotExited);

    QString renderProgram;
    if (_exporter.layout() == CFGExporter::Layout::Circular)
        renderProgram = QStringLiteral("twopi");
    else
        renderProgram = QStringLiteral("dot");

    QStringList renderArgs{QStringLiteral("-Tplain")};

    _renderProcessCmdLine = renderProgram + QLatin1Char(' ') + renderArgs.join(QLatin1Char(' '));

    qDebug("ControlFlogGraphView::refresh: Starting process %p, \'%s\'",
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

void ControlFlowGraphView::showText(const QString &text)
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
    QAction* a = m->addAction(s);

    a->setData(static_cast<int>(layout));
    a->setCheckable(true);
    a->setChecked(_exporter.layout() == layout);

    return a;
}

QMenu* ControlFlowGraphView::addPredecessorDepthMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(QObject::tr("Predecessor Depth"));
    QAction *a = addPredecessorDepthAction(m, QObject::tr("Unlimited"), -1);

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
    QAction *a = addSuccessorDepthAction(m, QObject::tr("Unlimited"), -1);

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
    QAction *a = addNodeLimitAction(m, QObject::tr("No Minimum"), 0.0);

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
    QMenu* m = menu->addMenu(QObject::tr("Birds-eye View"));

    addZoomPosAction(m, QObject::tr("Top Left"), ZoomPosition::TopLeft);
    addZoomPosAction(m, QObject::tr("Top Right"), ZoomPosition::TopRight);
    addZoomPosAction(m, QObject::tr("Bottom Left"), ZoomPosition::BottomLeft);
    addZoomPosAction(m, QObject::tr("Bottom Right"), ZoomPosition::BottomRight);
    addZoomPosAction(m, QObject::tr("Automatic"), ZoomPosition::Auto);
    addZoomPosAction(m, QObject::tr("Hide"), ZoomPosition::Hide);

    #if 0
    connect(m, &QMenu::triggered,
            this, &ControlFlowGraphView::zoomPosTriggered);
    #endif

    return m;
}

QMenu* ControlFlowGraphView::addLayoutMenu(QMenu* menu)
{
    QMenu* m = menu->addMenu(QObject::tr("Layout"));

    addLayoutAction(m, QObject::tr("Top to Down"), CFGExporter::Layout::TopDown);
    addLayoutAction(m, QObject::tr("Left to Right"), CFGExporter::Layout::LeftRight);
    addLayoutAction(m, QObject::tr("Circular"), CFGExporter::Layout::Circular);

    #if 0
    connect(m, &QMenu::triggered,
            this, &ControlFlowGraphView::layoutTriggered);
    #endif

    return m;
}

#ifndef PRODUCTIONCHAINFORM_H
#define PRODUCTIONCHAINFORM_H

#include <QWidget>
#include <QGraphicsView>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QGraphicsItem>
#include <QPair>
#include <qcombobox.h>
#include "core.h"
#include "itemstreamsmanagerform.h"
#include "productionnodes.h"

namespace Ui {
class PoductionChainForm;
}

class ConnectionItem;
class ProductionChainScene;
class ProductionChainForm;

class ProductionChainView : public QGraphicsView {
    Q_OBJECT
private:
    MASTERED(ProductionChainForm)

public:
    ProductionChainView(ProductionChainScene* scene, QWidget* parent = nullptr);

    void openEditionView(ProductionNode* node);
    void openConnectionView(ConnectionItem* node);

    void onNodeRemoval(ProductionNode* node);
protected:
    void wheelEvent(QWheelEvent* event) override {
        if (event->modifiers() & Qt::ControlModifier) {
            const double scaleFactor = 1.15;
            if (event->angleDelta().y() > 0) {
                scale(scaleFactor, scaleFactor);
            } else {
                scale(1.0 / scaleFactor, 1.0 / scaleFactor);
            }
        } else {
            QGraphicsView::wheelEvent(event);
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton) {
            setDragMode(QGraphicsView::ScrollHandDrag);
            QMouseEvent fakeEvent(event->type(), event->position(), event->scenePosition(),
                                  Qt::LeftButton, event->buttons() | Qt::LeftButton, event->modifiers());
            QGraphicsView::mousePressEvent(&fakeEvent);
        } else {
            QGraphicsView::mousePressEvent(event);
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton) {
            setDragMode(QGraphicsView::NoDrag);
        }
        QGraphicsView::mouseReleaseEvent(event);
    }
};

class ProductionNodeItem : public QGraphicsRectItem {
private:
    FULL_PROPERTY_PTR(ProductionChainScene*, master, nullptr, getMaster, setMaster, hasMaster);
    CONST_PROPERTY_PTR(ProductionNode*, node, getNode);
    CONST_PROPERTY_BIGPOD(QSet<ConnectionItem*>, connections, getConnections);

    QGraphicsTextItem* name = nullptr;
    QGraphicsTextItem* location = nullptr;
public:
    ProductionNodeItem(qreal x, qreal y, ProductionNode* node)
        : QGraphicsRectItem(QRectF{{0, 0}, node->getViewSize()}), node(node) {

        setPos(x, y);

        connections.reserve(5);

        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);

        setPen(QPen(QColor(200, 200, 200), 2));

        refreshNode();
    }
    ~ProductionNodeItem();

    void removeAllConnectionItems();

    /**
     * @brief getDockingPort DOIT RENVOYER EN COORDONNÉES ABSOLUES, par rapport à la scene donc
     * @param item
     * @return
     */
    QPointF getDockingPort(ConnectionItem* item);

    void setNode(ProductionNode* node) {
        this->node = node;
        refreshNode();
    }

    void refreshNode() {
        Util::clearGraphicsItem(this);

        setBrush(node->getColor());
        setRect({{0, 0}, node->getViewSize()});

        if (node != nullptr) {
            getNode()->design(this);
        } else {
            QGraphicsTextItem* item = new QGraphicsTextItem("ERROR", this);
            item->setPos(5, 2);
        }
    }

    void openEditionView();

private:
    friend ConnectionItem;
    friend ProductionChainScene;
    void _addConnection(ConnectionItem* item);
    void _removeConnection(ConnectionItem* item);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
};

class ConnectionItem : public QGraphicsPathItem {
private:
    ProductionNodeItem* source;
    ProductionNodeItem* dest;
    NodeConnection conn;

public:
    ConnectionItem(ProductionNodeItem* source, ProductionNodeItem* dest)
        : conn(source->getNode()->getChildConnection(dest->getNode())), source(source), dest(dest) {


        QPen pen(dest->getNode() == nullptr ? Qt::white :  dest->getNode()->getColor(), 3);
        pen.setCapStyle(Qt::RoundCap);  // Bouts arrondis       | J'ai mis parce que c'était dans le tuto, mais bon... x)
        pen.setJoinStyle(Qt::RoundJoin); // Jonctions arrondies |
        setPen(pen);
        setFlag(QGraphicsItem::ItemIsSelectable, true);

        source->_addConnection(this);
        dest->_addConnection(this);

        setZValue(-5);

        updatePath();
    }

    inline const NodeConnection& getConnection() const { return conn; }

    inline ProductionChainScene* getMaster() { return source->hasMaster() ? source->getMaster() : dest->getMaster(); }
    inline bool hasMaster() { return getMaster() != nullptr; }

    ~ConnectionItem() {
        if (source != nullptr) source->_removeConnection(this);
        if (dest != nullptr) dest->_removeConnection(this);
    }

    void updatePath() {
        if (!source || !dest) return;

        QPointF startPoint = source->getDockingPort(this);
        QPointF endPoint = dest->getDockingPort(this);

        QPainterPath path;
        path.moveTo(startPoint);

        qreal dx = endPoint.x() - startPoint.x();
        QPointF ctrlPoint1(startPoint.x() + dx * 0.5, startPoint.y());
        QPointF ctrlPoint2(endPoint.x() - dx * 0.5, endPoint.y());
        path.cubicTo(ctrlPoint1, ctrlPoint2, endPoint);

        constexpr qreal t = 0.5;

        QPointF center = path.pointAtPercent(t);

        qreal angleDeg = path.angleAtPercent(t);
        qreal angleRad = -angleDeg * M_PI / 180.0;

        constexpr qreal arrowSize = 10.0;

        QPointF dir(std::cos(angleRad), std::sin(angleRad));
        QPointF perp(-dir.y(), dir.x());

        QPointF tip = center + dir * arrowSize;

        QPointF left =
            center - dir * arrowSize * 0.5
            + perp * arrowSize * 0.5;

        QPointF right =
            center - dir * arrowSize * 0.5
            - perp * arrowSize * 0.5;

        path.moveTo(left);
        path.lineTo(tip);
        path.lineTo(right);


        setPath(path);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

    void swapNode(ProductionNodeItem* previous, ProductionNodeItem* instead) {
        if (source == previous) source = instead;
        else if (dest == previous) dest = instead;
    }

    inline ProductionNodeItem* getSource() { return source; }
    inline ProductionNodeItem* getDestination() { return dest; }
    inline ProductionNodeItem* getOtherNode(ProductionNodeItem* node) { return node == source ? dest : source; }
};

class ProductionChainScene : public QGraphicsScene {
private:
    QHash<ProductionNode*, ProductionNodeItem*> items = {};
    QSet<QPair<ProductionNode*, ProductionNode*>> builtLinks = {};
    QSet<QPair<ProductionNode*, ProductionNode*>> unconnectedLinks = {};
    ProductionChain* prodChain;
    qreal defaultXNodeSpacing = 50;
    qreal defaultYNodeSpacing = 30;
    ProductionNodeItem* linkStartNode = nullptr;
    QGraphicsPathItem* previewLink = nullptr;
    bool linkingMode = false;
    MASTERED(ProductionChainView)

public:
    ProductionChainScene(ProductionChain* prodChain, QObject* parent = nullptr) : QGraphicsScene(parent), prodChain(prodChain) {
    }

    /**
     * @brief rebuildChainView Très lourd, recommence tout l'affichage.
     */
    void rebuildChainView();

    /**
     * @brief addProductionItem WON'T ADD IT TO prodChain, JUST TO THE VIEW, it is not a "create" method
     * @param item
     * @param x
     * @param y
     * @param addToChain
     */
    ProductionNodeItem* addProductionItem(ProductionNode* item, qreal x, qreal y, bool addToChain = false);

    void refreshSpecificNode(ProductionNode* node);

    void updatePreviewLink(const QPointF& scenePos);
    void startLink(ProductionNodeItem* node);
    void cancelLink();

    void resetBuildingVariables();

    ProductionNode* createNodeFromParent(ProductionNode* parent);
    ProductionNode* createNodeFromChild(ProductionNode* child);
    void removeNode(ProductionNode* item);
    void removeNodeView(ProductionNodeItem* view, bool removeFromMap = true);
    void removeConnection(ConnectionItem* conn);

    void _addConnection(ProductionNode* a, ProductionNode* b) {
        if (builtLinks.contains(QPair{a, b})) return;
        ConnectionItem* item = new ConnectionItem(items.value(a), items.value(b));
        builtLinks.insert(QPair{a, b});
        this->addItem(item);
    }

    void deleteAllViews() {
        for (auto [k, v]: items.asKeyValueRange()) {
            removeNodeView(v, false);
        }
        items.clear();
    }

    void linkRequest(ProductionNodeItem* child);
    inline bool isLinking() const { return linkingMode; }

    void openEditionView(ProductionNode* node);
    void openConnectionEditionView(ConnectionItem* connection);

    inline ProductionChain* getProductionChain() const { return prodChain; }
protected:
    double buildLine(ProductionNode* node, double xstart, double maxY);

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
};

class ProductionNodeEditionWidget : public QWidget {
    Q_OBJECT

private:
    ProductionNode* node = nullptr;
    QWidget* content;
    QLabel* nodeName;
    QComboBox* nodeTypes;
    QMap<QString, std::function<QVariant()>> properties;
    QMap<QString, QVariant> workingMemory; // Pour que les node types puissent stocker des elts dedans
    QStringList propertiesOrder;
    ItemStreamsManagerForm* streamManagerForm = nullptr;
    MASTERED(ProductionChainForm);

    std::function<void(ProductionNode*)> preLoad = [] (ProductionNode*) {},
        edit = [](ProductionNode*) {},
        postLoad = [] (ProductionNode*) {};

public:
    ProductionNodeEditionWidget(QWidget* widget);
    ~ProductionNodeEditionWidget() { delete streamManagerForm; }

    inline void setPreLoader(std::function<void(ProductionNode*)> preLoad) {
        this->preLoad = preLoad;
    }
    inline void setSubmitterFunction(std::function<void(ProductionNode*)> editor) {
        this->edit = editor;
    }
    inline void setPostLoader(std::function<void(ProductionNode*)> postLoad) {
        this->postLoad = postLoad;
    }

    inline bool hasNode() const { return node != nullptr; }
    inline ProductionNode* getNode() const { return node; }
    inline void clearProperties() { properties.clear(); propertiesOrder.clear(); }
    inline QWidget* getContentWidget() const { return content; }
    inline void linkProperty(const QString& property, const std::function<QVariant()> &executor) {
        if (properties.contains(property)) propertiesOrder.removeAll(property);
        propertiesOrder.append(property);
        properties.insert(property, executor);
    }
    void setNode(ProductionNode* node);
    void refreshNode();

    void closeIfNodeIs(ProductionNode* node) {
        if (this->node == node) {
            setNode(nullptr);
        }
    }

    void submitNode() {
        node->setType(ProductionNodeTypes::fromName(nodeTypes->currentText()));

        preLoad(node);

        edit(node);

        postLoad(node);

        refreshGlobal();
    }

    void refreshGlobal();

    void openStreamEditionView();

    QMap<QString, QVariant>& workingMemoryRef() { return workingMemory; }
    inline void setWorkingMemory(const QMap<QString, QVariant>& map) { this->workingMemory = map; }

private slots:
    void onStreamSave();
};

class NodeConnectionEditionWidget : public QWidget {
    Q_OBJECT
private:
    GENERAL_PROPERTY_BIGPOD(NodeConnection, conn, {}, getConnection, setConnection);
    FULL_PROPERTY_PTR(ConnectionItem*, connItem, nullptr, getConnectionItem, setConnectionItem, hasConnectionItem)
    MASTERED(ProductionChainForm)

public:
    NodeConnectionEditionWidget(QWidget* widget);

    void setup(const NodeConnection& conn, ConnectionItem* item = nullptr) {
        setConnection(conn);
        setConnectionItem(item);

        refresh();
    }

    void refresh();
};

class ProductionChainForm : public QWidget
{
    Q_OBJECT

public:
    explicit ProductionChainForm(QWidget *parent = nullptr);
    ~ProductionChainForm();

    void refreshAll();
    void openEditionView(ProductionNode* node);
    void openConnectionView(const NodeConnection& conn, ConnectionItem* connItem = nullptr);
    void onNodeRemoval(ProductionNode* node);
    void notifyNodeChanges(ProductionNode* node);

    inline ProductionChain* getChain() const { return scene->getProductionChain(); }
private slots:
    void on_pushButton_refresh_clicked();

    void on_pushButton_saveChain_clicked();

private:
    Ui::PoductionChainForm *ui;
    ProductionChainView* view;
    ProductionChainScene* scene;
    ProductionNodeEditionWidget* nodeEditionView;
    NodeConnectionEditionWidget* nodeConnectionView;
    QWidget* chainMonitor;
};

#endif // PRODUCTIONCHAINFORM_H

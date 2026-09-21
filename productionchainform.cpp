#include "productionchainform.h"
#include "ui_productionchainform.h"
#include "core.h"
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QSplitter>

ProductionChainForm::ProductionChainForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PoductionChainForm)
{
    ui->setupUi(this);



    ProductionChain* chain = new ProductionChain(1, "Test Chain 1", "Description de test");
    ProductionNode* n2 = new ProductionNode(ProductionNodeTypes::MANUFACTURE, Stations::fromName("Jita 4-4"));
    ProductionNode* n3 = new ProductionNode(ProductionNodeTypes::MANUFACTURE, Stations::fromName("Jita 4-4"));
    ProductionNode* n5 = new ProductionNode(ProductionNodeTypes::SELL, Stations::fromName("Jita 4-4"));

    chain->addRawProductionNode(n2, true, true);
    chain->addChilProductionNode(n3, n2);
    chain->addChilProductionNode(n5, n3);
    ItemStack bpc = {Items::fromName("Life Support Backup Unit Blueprint"), 1};
    bpc.setBpc(true);
    n2->setMetadata(ProductionNodeProperties::MAIN_BLUEPRINT, QVariant::fromValue(bpc));
    n3->setMetadata(ProductionNodeProperties::MAIN_BLUEPRINT, QVariant::fromValue(ItemStack{Items::fromName("Vexor Navy Issue Blueprint", true), 1}));


    new QHBoxLayout(ui->widget_content);
    ui->widget_content->layout()->setContentsMargins(0, 0, 0, 0);
    QSplitter* splitter = new QSplitter(ui->widget_content);
    ui->widget_content->layout()->addWidget(splitter);

    QSplitter* w0 = new QSplitter(Qt::Horizontal);

    scene = new ProductionChainScene(chain);
    scene->rebuildChainView();
    view = new ProductionChainView(scene, w0);
    view->setMaster(this);
    view->setStyleSheet("background-color: rgb(171, 171, 171);");

    nodeEditionView = new ProductionNodeEditionWidget(w0);
    nodeEditionView->setMaster(this);

    nodeConnectionView = new NodeConnectionEditionWidget(w0);
    nodeConnectionView->setMaster(this);

    w0->addWidget(view);
    w0->addWidget(nodeConnectionView);
    w0->addWidget(nodeEditionView);

    nodeEditionView->hide();
    nodeConnectionView->hide();

    splitter->addWidget(w0);

    chainMonitor = new QWidget();
    splitter->addWidget(chainMonitor);
}

void ProductionChainForm::openEditionView(ProductionNode* node) {
    nodeEditionView->setNode(node);
    nodeEditionView->hide();
    nodeEditionView->show();
}

void ProductionChainForm::openConnectionView(const NodeConnection& conn, ConnectionItem* connItem) {
    nodeConnectionView->setup(conn, connItem);
    nodeConnectionView->hide();
    nodeConnectionView->show();
}

ProductionNodeItem::~ProductionNodeItem() {
    removeAllConnectionItems();
}

void ProductionChainScene::openConnectionEditionView(ConnectionItem* connection) {
    if (hasMaster()) getMaster()->openConnectionView(connection);
}

void ProductionNodeItem::removeAllConnectionItems() {
    for (ConnectionItem* conn: getConnections()) {
        if (hasMaster()) getMaster()->removeConnection(conn);
        else delete conn;
    }
}

ProductionChainView::ProductionChainView(ProductionChainScene* scene, QWidget* parent) : QGraphicsView(scene, parent) {
    scene->setMaster(this);

    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::TextAntialiasing);

    setDragMode(QGraphicsView::NoDrag);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ProductionNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    auto* view = event->widget();
    QMenu menu(view);

    QAction* actionEdit = menu.addAction("Edit Node");
    QAction* actionAddChild = menu.addAction("Add Child");
    QAction* actionAddParent = menu.addAction("Add Parent");
    QAction* actionLinkChild = menu.addAction("Link Child");
    QAction* actionDelete = menu.addAction("Delete Node");

    QObject::connect(actionEdit, &QAction::triggered, [this] () {
        openEditionView();
    });

    QObject::connect(actionAddChild, &QAction::triggered, [this]() {
        getMaster()->createNodeFromParent(this->node);
    });

    QObject::connect(actionAddParent, &QAction::triggered, [this]() {
        getMaster()->createNodeFromChild(this->node);
    });

    QObject::connect(actionDelete, &QAction::triggered, [this]() {
        getMaster()->removeNode(this->node);
    });

    QObject::connect(actionLinkChild, &QAction::triggered, [this] () {
        getMaster()->startLink(this);
    });

    menu.exec(event->screenPos());

    event->accept();
}

void ProductionNodeItem::openEditionView() {
    if (hasMaster()) getMaster()->openEditionView(getNode());
}

void ProductionChainScene::openEditionView(ProductionNode* node) {
    if (hasMaster()) getMaster()->openEditionView(node);
}

void ProductionChainView::openEditionView(ProductionNode* node) {
    if (hasMaster()) getMaster()->openEditionView(node);
}

void ProductionChainView::openConnectionView(ConnectionItem *conn) {
    if (hasMaster()) getMaster()->openConnectionView(conn->getConnection(), conn);
}

void ProductionChainScene::removeNode(ProductionNode* node) {
    if (node == nullptr) return;
    ProductionNodeItem* graphics = items[node];
    removeNodeView(graphics);

    if (hasMaster()) getMaster()->onNodeRemoval(node);
    prodChain->removeProductionNode(node, true);
}

void ProductionChainView::onNodeRemoval(ProductionNode* node) {
    if (hasMaster()) getMaster()->onNodeRemoval(node);
}

void ProductionChainForm::onNodeRemoval(ProductionNode* node) {
    nodeEditionView->closeIfNodeIs(node);
}

void ProductionChainScene::removeNodeView(ProductionNodeItem* view, bool removeFromMap) {
    if (view != nullptr) {
        removeItem(view);
        items.remove(view->getNode());
        delete view;
    }
}

void ProductionChainScene::removeConnection(ConnectionItem* conn) {
    if (conn == nullptr) return;
    conn->getSource()->_removeConnection(conn);
    conn->getDestination()->_removeConnection(conn);

    removeItem(conn);
    delete conn;
}

void ProductionChainScene::refreshSpecificNode(ProductionNode* node) {
    if (items.contains(node)) {
        items[node]->refreshNode();
    }
}

ProductionNode* ProductionChainScene::createNodeFromParent(ProductionNode* parent) {
    ProductionNode* node = new ProductionNode(ProductionNodeTypes::EMPTY_NODE, nullptr);

    prodChain->addChilProductionNode(node, parent);

    ProductionNodeItem* parentGraphics = items[parent];
    if (parentGraphics != nullptr) {
        QRectF rect = parentGraphics->sceneBoundingRect();
        addProductionItem(node, rect.right() + 50, rect.y());
    } else {
        Util::error("No graphics for parent !!!");
    }

    return node;
}

ProductionNode* ProductionChainScene::createNodeFromChild(ProductionNode* child) {
    ProductionNode* node = new ProductionNode(ProductionNodeTypes::EMPTY_NODE, nullptr);

    prodChain->addParentProductionNode(node, child);

    ProductionNodeItem* parentGraphics = items[child];
    if (parentGraphics != nullptr) {
        QRectF rect = parentGraphics->sceneBoundingRect();
        addProductionItem(node, rect.right() - defaultXNodeSpacing, rect.y());
    } else {
        Util::error("No graphics for parent !!!");
    }

    return node;
}

QVariant ProductionNodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        for (ConnectionItem* connection : std::as_const(connections)) {
            connection->updatePath();
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

QPointF ProductionNodeItem::getDockingPort(ConnectionItem* conn) {
    return this->sceneBoundingRect().center();
}

void ProductionNodeItem::_addConnection(ConnectionItem* conn) {
    connections.insert(conn);
}

void ProductionNodeItem::_removeConnection(ConnectionItem* conn) {
    connections.remove(conn);
    conn->swapNode(this, nullptr);
}

ProductionChainForm::~ProductionChainForm()
{
    delete ui;
}

void ProductionChainScene::rebuildChainView() {
    deleteAllViews();
    resetBuildingVariables();
    double lowestY = 0.;
    for (ProductionNode* node: prodChain->getInputs()) {
        Util::println("Building sth");
        lowestY = buildLine(node, 0., lowestY);
        lowestY += items[node]->sceneBoundingRect().height() + defaultYNodeSpacing;
    }

    for (const QPair<ProductionNode*, ProductionNode*>& pair: unconnectedLinks) {
        if (items.contains(pair.first) && items.contains(pair.second)) {
            _addConnection(pair.first, pair.second);
        } else {
            return Util::error("Anormal Situation 0001");
        }
    }
}

void ProductionChainScene::resetBuildingVariables() {
    unconnectedLinks.clear();
    builtLinks.clear();
}

double ProductionChainScene::buildLine(ProductionNode* parentNode, double xstart, double nextY) {
    if (parentNode == nullptr) return nextY;
    if (items.contains(parentNode)) {
        return nextY;
    }
    ProductionNodeItem* parentGraphics = addProductionItem(parentNode, xstart, nextY);
    QRectF parentRect = parentGraphics->sceneBoundingRect();
    bool start = true;
    for (const NodeConnection& conn: parentNode->getChildren()) {
        nextY = buildLine(conn.getChild(), xstart + parentGraphics->boundingRect().width() + defaultXNodeSpacing, nextY);
        nextY = items[conn.getChild()]->sceneBoundingRect().bottom() + defaultYNodeSpacing;
    }
    return nextY;
}

void ProductionNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::ShiftModifier) {
            if (hasMaster() && !getMaster()->isLinking()) {
                getMaster()->startLink(this);
                event->accept();
                return;
            }
        } else if (event->modifiers() & Qt::AltModifier) {
            openEditionView();
        }
        if (hasMaster() && getMaster()->isLinking()) {
            getMaster()->linkRequest(this);
            event->accept();
            return;
        }
    }

    QGraphicsRectItem::mousePressEvent(event);
}


void ConnectionItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::AltModifier) {
            if (hasMaster()) {
                getMaster()->openConnectionEditionView(this);

                event->accept();
                return;
            }
        }
    } else if (event->button() == Qt::RightButton) {
        if (hasMaster()) {
            ProductionNode* node = conn.getParent();
            ProductionChainScene* master = getMaster();

            source->getNode()->removeChild(dest->getNode());

            QTimer::singleShot(0, [node, master] () { master->refreshSpecificNode(node); });
            getMaster()->removeConnection(this);
            return;
        }
    }

    QGraphicsPathItem::mousePressEvent(event);
}

ProductionNodeItem* ProductionChainScene::addProductionItem(ProductionNode* item, qreal x, qreal y, bool addToChain) {
    if (item == nullptr) return nullptr;
    if (items.contains(item)) {
        removeNodeView(items[item]);
    }

    ProductionNodeItem* graphicItem = new ProductionNodeItem(x, y, item);
    graphicItem->setMaster(this);
    items.insert(item, graphicItem);
    addItem(graphicItem);


    for (const NodeConnection& conn: item->getParents()) {
        if (items.contains(conn.getParent())) {
            _addConnection(conn.getParent(), item);
        } else {
            unconnectedLinks.insert({conn.getParent(), item});
        }
    }
    for (const NodeConnection& conn: item->getChildren()) {
        if (items.contains(conn.getChild())) {
            _addConnection(item, conn.getChild());
        } else {
            unconnectedLinks.insert({item, conn.getChild()});
        }
    }

    return graphicItem;
}

void ProductionChainScene::startLink(ProductionNodeItem* node) {
    linkingMode = true;
    linkStartNode = node;

    previewLink = new QGraphicsPathItem();
    QPen pen(Qt::DashLine);
    pen.setColor(Qt::yellow);
    pen.setWidth(2);
    previewLink->setPen(pen);

    addItem(previewLink);
}

void ProductionChainScene::cancelLink() {
    linkingMode = false;
    linkStartNode = nullptr;

    if (previewLink) {
        removeItem(previewLink);
        delete previewLink;
        previewLink = nullptr;
    }
}

void ProductionChainScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (linkingMode) {
        updatePreviewLink(event->scenePos());
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void ProductionChainScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (linkingMode) {

        if (event->button() == Qt::RightButton) {
            cancelLink();
            return;
        }

        if (event->button() == Qt::LeftButton) {
            QGraphicsItem* item = itemAt(event->scenePos(), QTransform());
            auto* nodeItem = dynamic_cast<ProductionNodeItem*>(item);
            linkRequest(nodeItem);
        }
    }

    QGraphicsScene::mousePressEvent(event);
}

void ProductionChainScene::linkRequest(ProductionNodeItem* nodeItem) {
    if (linkingMode) if (nodeItem && nodeItem != linkStartNode) {
        if (hasMaster()) getMaster()->getMaster()->getChain()->addChilProductionNode(linkStartNode->getNode(), nodeItem->getNode());
        _addConnection(linkStartNode->getNode(), nodeItem->getNode());
        cancelLink();
        return;
    }
}

void ProductionChainScene::updatePreviewLink(const QPointF& scenePos) {
    if (!previewLink || !linkStartNode) return;

    QPointF start = linkStartNode->sceneBoundingRect().center();
    QPointF end = scenePos;

    QPainterPath path;
    path.moveTo(start);

    qreal dx = end.x() - start.x();
    QPointF c1(start.x() + dx * 0.5, start.y());
    QPointF c2(end.x() - dx * 0.5, end.y());
    path.cubicTo(c1, c2, end);

    previewLink->setPath(path);
}

void ProductionNodeEditionWidget::setNode(ProductionNode* node) {
    this->node = node;
    refreshNode();
}

void ProductionNodeEditionWidget::onStreamSave() {

}

ProductionNodeEditionWidget::ProductionNodeEditionWidget(QWidget* widget) : QWidget(widget) {
    new QVBoxLayout(this);

    streamManagerForm = new ItemStreamsManagerForm;
    connect(streamManagerForm, &ItemStreamsManagerForm::streamSaved, this, &ProductionNodeEditionWidget::onStreamSave);

    QWidget* header = new QWidget(this);
    layout()->addWidget(header);
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    new QHBoxLayout(header);
    header->layout()->setContentsMargins(0, 0, 0, 0);

    nodeName = new QLabel(header);
    header->layout()->addWidget(nodeName);
    nodeTypes = new QComboBox(header);
    for (ProductionNodeType* type: ProductionNodeTypes::VALUES) {
        nodeTypes->addItem(type->getName());
    }
    header->layout()->addWidget(nodeTypes);
    QPushButton* button = new QPushButton("Close", header);
    connect(button, &QPushButton::clicked, this, [this] () { hide(); });
    header->layout()->addWidget(button);

    QScrollArea* contentArea = new QScrollArea(this);
    this->layout()->addWidget(contentArea);
    contentArea->setWidgetResizable(true);
    contentArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

    content = new QWidget(contentArea);
    contentArea->setWidget(content);
    content->setStyleSheet("background-color: rgb(71, 71, 71);");
    new QVBoxLayout(content);
    content->layout()->setContentsMargins(5, 5, 5, 5);

    QWidget* footer = new QWidget(this);
    layout()->addWidget(footer);
    (new QHBoxLayout(footer))->setContentsMargins(0, 0, 0, 0);
    QPushButton* editStream = new QPushButton("Edit Fluxs", footer);
    footer->layout()->addWidget(editStream);
    connect(editStream, &QPushButton::clicked, this, &ProductionNodeEditionWidget::openStreamEditionView);

    QPushButton* save = new QPushButton("Save", footer);
    footer->layout()->addWidget(save);
    connect(save, &QPushButton::clicked, this, &ProductionNodeEditionWidget::submitNode);
}

void onStreamSave() {

}

NodeConnectionEditionWidget::NodeConnectionEditionWidget(QWidget* wdg) : QWidget{wdg}, conn() {
    new QVBoxLayout(this);

    ADD_LAYOUTED_WIDGET(header, QHBoxLayout, this);

    header->layout()->addWidget(new QLabel("Connection Node"));
    QPushButton* closeButton = new QPushButton("Close", header);
    header->layout()->addWidget(closeButton);
    connect(closeButton, &QPushButton::clicked, this, &NodeConnectionEditionWidget::hide);

    layout()->addWidget(header);
}

void ProductionNodeEditionWidget::openStreamEditionView() {
    streamManagerForm->setNode(node);
    if (!streamManagerForm->hasFocus()) streamManagerForm->hide();
    streamManagerForm->show();
}

void NodeConnectionEditionWidget::refresh() {

}

void ProductionNodeEditionWidget::refreshNode() {
    nodeName->setText(hasNode() ? getNode()->getName() : "");
    Util::clearChildren(content);
    clearProperties();
    workingMemory.clear();

    nodeTypes->setCurrentText(hasNode() ? getNode()->getType()->getName() : ProductionNodeTypes::EMPTY_NODE->getName());

    if (hasNode()) getNode()->designEditionWidget(this);
}

void ProductionNodeEditionWidget::refreshGlobal() {
    if (!hasNode()) return;
    node->getProductionChain()->updateNodeIODatasMap();
    refreshNode();

    if (hasMaster()) getMaster()->notifyNodeChanges(getNode());
}

void ProductionChainForm::notifyNodeChanges(ProductionNode* node) {
    scene->refreshSpecificNode(node);
}

void ProductionChainForm::refreshAll() {
    scene->getProductionChain()->updateNodeIODatasMap();
    scene->rebuildChainView();
}

void ProductionChainForm::on_pushButton_refresh_clicked()
{
    refreshAll();
}


void ProductionChainForm::on_pushButton_saveChain_clicked()
{
    getChain()->saveToDB();
}


#include "productionnodes.h"
#include "itemwidget.h"
#include "productionchainform.h"
#include <QGraphicsTextItem>
#include <QGraphicsProxyWidget>
#include <QGraphicsLayout>
#include <QLineEdit>
#include <qcheckbox.h>
#include <qheaderview.h>
#include <qpushbutton.h>
#include "esimanager.h"
#include <qspinbox.h>
#include <QPointer>
#include <qtablewidget.h>
#include "productionnodes.h"

class DefaultProductionNodeType : public ProductionNodeType {
private:
    CONST_CONSTRUCTABLE_PROPERTY_POD(QColor, color, getColor);
    GENERAL_PROPERTY_BIGPOD(QSizeF, viewDimensions, {}, getViewSize, setViewSize);

public:
    DefaultProductionNodeType(int id, const QString &name, const QString& tableName, const QSet<ProductionNodeProperty*> &properties, const QColor &color) : ProductionNodeType(id, name, tableName, properties),
        color(color),
        viewDimensions(QSizeF{150, 67})
    {}


    void designNodeItem(ProductionNodeItem* item) override {
        ProductionNode *node = item->getNode();
        if (node == nullptr) return Util::error("Node is nullptr");

        QGraphicsTextItem* label = new QGraphicsTextItem(node->getType()->getName(), item);
        label->setFont(QFont("Segoe UI", 15));
        QPointer<GraphicsTextItemObject> deltaZ = new GraphicsTextItemObject(item);
        deltaZ->setPlainText("ΔZ: unknown");
        node->fetchTotalNetCost(node->getCharacter(), [deltaZ] (double v) {
            if (deltaZ) {
                deltaZ->setPlainText("ΔZ: " + moneyToString(v, false));
            }
        });
        QGraphicsTextItem* station = new QGraphicsTextItem(node->hasLocation() ? "In " + node->getLocation()->getName() : "Unknown location", item);

        label->setPos(5, 2);
        station->setPos(5, 27);
        deltaZ->setPos(5, 41);
    }

    int getOutputQuantity(ProductionNode* node, const ItemStackDatas& datas) override { return 0; }
    int getInputQuantity(ProductionNode* node, const ItemStackDatas& datas) override { return 0; }
    QSet<ItemStack> getOutputs(ProductionNode* node) override {
        return {};
    }

    void designEditionView(ProductionNodeEditionWidget* parent, ProductionNode* node) override {
        parent->clearProperties();
    }

    QSizeF getNodeSize(const ProductionNode* node) override {
        return getViewSize();
    }

    QColor getNodeColor(const ProductionNode* node) override {
        return getColor();
    }

    QString getNodeLabel(const ProductionNode* node) override {
        return getName();
    }

    void insertDBLine(ProductionNode* node) override {

    }

    void loadDBLine(ProductionNode* node) override {

    }

    QMap<QString, QSet<ItemView>> requiredItems(const ProductionNode* node) override {
        return {};
    }

    void fetchTotalPrice(ProductionNode* node, Character* character, std::function<void(double)> recv) override {
        recv(0);
    }
};

class SingleOutputProductionNodeType : public DefaultProductionNodeType {
public:
    SingleOutputProductionNodeType(int id, const QString &name, const QString& tableName, const QSet<ProductionNodeProperty *> &properties, const QColor &color)
        : DefaultProductionNodeType(id, name, tableName, properties, color) {
        setViewSize({170, 110});
    }

    void designNodeItem(ProductionNodeItem* item) override {
        DefaultProductionNodeType::designNodeItem(item);
        ProductionNode* node = item->getNode();


        QGraphicsProxyWidget* itemLine = new QGraphicsProxyWidget(item);

        QFrame* holder = new QFrame();
        new QVBoxLayout(holder);
        holder->layout()->setContentsMargins(1, 1, 1, 1);
        holder->setFrameShadow(QFrame::Plain);
        holder->setFrameShape(QFrame::Box);
        holder->setObjectName("itemBoxFrame");
        holder->setStyleSheet("QFrame#itemBoxFrame { border: 1px solid white; }");
        holder->setFixedSize(160, 30);

        LinedItemWidgetV2* widget = new LinedItemWidgetV2(holder, true);
        widget->limitIconSize(50);
        //widget->setItem(node->getOutput());
        widget->refresh();
        holder->layout()->addWidget(widget);

        itemLine->setWidget(holder);
        itemLine->setPos(5, 70);
    }

    void designEditionView(ProductionNodeEditionWidget* parent, ProductionNode* node) override {
        parent->clearProperties();

        Util::println("Called design");
        QWidget* temp = new QWidget(parent->getContentWidget());
        parent->getContentWidget()->layout()->addWidget(temp);
        new QHBoxLayout(temp);
        temp->layout()->setContentsMargins(0, 0, 0, 0);
        QLineEdit* outputLineEdit = new QLineEdit(temp);
        outputLineEdit->setPlaceholderText("Write Output Name here");
        QSpinBox* outputQuantity = new QSpinBox(temp);
        outputQuantity->setMaximum(1e9);
        outputQuantity->setMinimum(1);
/**
        if (node != nullptr && node->getOutput().isValid()) {
            outputLineEdit->setText(node->getOutput().getItem()->getName());
            outputQuantity->setValue(node->getOutput().getQuantity());
        } else {
            outputLineEdit->clear();
            outputQuantity->setValue(1);
        }*/

        temp->layout()->addWidget(outputLineEdit);
        temp->layout()->addWidget(outputQuantity);


        QPushButton* checkOutput = new QPushButton("Check Output", parent->getContentWidget());
        QObject::connect(checkOutput, &QPushButton::clicked, [outputLineEdit] () {
            Item *item = Items::fromName(outputLineEdit->text());
            if (item == nullptr) outputLineEdit->clear();
        });
        parent->getContentWidget()->layout()->addWidget(checkOutput);

        /*parent->linkProperty(OUTPUT_SINGLEITEM_PNKEY, [outputLineEdit, outputQuantity] () {
            return QVariant::fromValue(ItemStack(Items::fromName(outputLineEdit->text()), outputQuantity->value()));
        });*/
    }

    QString getNodeLabel(const ProductionNode* node) override {
        return getName();
    }
};

class ManufactureNodeType : public DefaultProductionNodeType {
public:
    ManufactureNodeType(int id, const QString &name, const QString& tableName, const QSet<ProductionNodeProperty *> &properties, const QColor &color)
        : DefaultProductionNodeType(id, name, tableName, properties, color) {
        addProperty(ProductionNodeProperties::MAIN_BLUEPRINT);
        addProperty(ProductionNodeProperties::MATERIALS_INPUTS);
        setViewSize({170, 110});
    }

    Blueprint* getBlueprint(ProductionNode* node) {
        return node->getMetadataRef(ProductionNodeProperties::MAIN_BLUEPRINT).value<ItemStack>().getBlueprint();
    }

    QMap<ItemStackDatas, int> probeMissingItems(ProductionNode* node) {
        if (!node->hasProductionChain()) return getBlueprint(node)->getRecipeInput();

        Blueprint* work = getBlueprint(node);
        QMap<ItemStackDatas, int> res = work->getRecipeInput();
        QList<ItemStack> received = node->getProductionChain()->getNodeIODatas().getUnsortedInputs(node)[ProductionNodeProperties::MATERIALS_INPUTS];
        for (ItemStack i: received) {
            ItemStackDatas w = i.getDatas();
            if (res.contains(w)) {
                res[w] -= i.getAmount();
            }
        }

        for (const auto& [k, v]: res.asKeyValueRange()) {
            QString name = k.isValid() ? k.getItem()->getName() : "Invalid";
            if (v <= 0) {
                res.remove(k);
            }
        }

        return res;
    }

    void designEditionView(ProductionNodeEditionWidget* parent, ProductionNode* node) override {
        Util::println("Designing start");
        parent->clearProperties();

        ADD_LAYOUTED_WIDGET(blueprintDatasWidget, QVBoxLayout, parent->getContentWidget());
        blueprintDatasWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        ADD_LAYOUTED_WIDGET(selectBlueprintWidget, QHBoxLayout, blueprintDatasWidget);
        ADD_LAYOUTED_WIDGET(blueprintModifiersWidget, QVBoxLayout, blueprintDatasWidget);
        ADD_LAYOUTED_WIDGET(materialEfficiencyWidget, QHBoxLayout, blueprintModifiersWidget);
        ADD_LAYOUTED_WIDGET(timeEfficiencyWidget, QHBoxLayout, blueprintModifiersWidget);
        ADD_LAYOUTED_WIDGET(blueprintTypeWidget, QHBoxLayout, blueprintModifiersWidget);

        //! ---------------------------------------------------------
        //! Select Blueprint
        QLineEdit* lineEditName = new QLineEdit(selectBlueprintWidget);
        lineEditName->setPlaceholderText("Type the blueprint name");
        selectBlueprintWidget->layout()->addWidget(lineEditName);

        //! Blueprint Modifiers
        // Material
        QLabel* materialLabel = new QLabel("Material Efficiency (%)", materialEfficiencyWidget);
        QDoubleSpinBox* materialEfficiency = new QDoubleSpinBox(materialEfficiencyWidget);
        materialEfficiencyWidget->layout()->addWidget(materialLabel);
        materialEfficiencyWidget->layout()->addWidget(materialEfficiency);

        // Time
        QLabel* timeLabel = new QLabel("Time Efficiency (%)", timeEfficiencyWidget);
        QDoubleSpinBox* timeEfficiency = new QDoubleSpinBox(timeEfficiencyWidget);
        timeEfficiencyWidget->layout()->addWidget(timeLabel);
        timeEfficiencyWidget->layout()->addWidget(timeEfficiency);

        // is copy
        QCheckBox* isBPC = new QCheckBox("Is BPC", blueprintTypeWidget);
        blueprintTypeWidget->layout()->addWidget(isBPC);


        parent->setSubmitterFunction([lineEditName, materialEfficiency, timeEfficiency, isBPC] (ProductionNode* node) {
            ItemStack work;
            work.setItem(Items::fromName(lineEditName->text(), true));
            work.setQuantity(1);
            work.setBlueprintMaterialsModifier(1. - (materialEfficiency->value() / 100.));
            work.setBlueprintTimeModifier(1. - (timeEfficiency->value() / 100.));
            work.setBpc(isBPC->isChecked());
            if (work.isValid() && work.isBlueprint()) {
                node->setMetadata(ProductionNodeProperties::MAIN_BLUEPRINT, QVariant::fromValue(work));
            } else {
                node->setMetadata(ProductionNodeProperties::MAIN_BLUEPRINT, QVariant::fromValue(ItemStack()));
            }
        });

        //! Button at the bottom
        QPushButton* checkBlueprint = new QPushButton("Check Blueprint", blueprintDatasWidget);
        blueprintDatasWidget->layout()->addWidget(checkBlueprint);
        QObject::connect(checkBlueprint, &QPushButton::clicked, [lineEditName] () {
            Item* item = Items::fromName(lineEditName->text());
            if (item == nullptr || !item->isBlueprint()) lineEditName->clear();
        });
        //! ---------------------------------------------------------


        //! ---------------------------------------------------------
        ADD_LAYOUTED_WIDGET(sep1, QVBoxLayout, parent->getContentWidget());
        sep1->setMaximumHeight(50);
        ADD_LAYOUTED_WIDGET(blueprintInformations, QVBoxLayout, parent->getContentWidget());
        QLabel* label = new QLabel("Output", blueprintInformations);
        blueprintInformations->layout()->addWidget(label);
        LinedItemWidgetV2* itemWidget = new LinedItemWidgetV2(blueprintInformations, true);
        itemWidget->setItem(Items::fromName(lineEditName->text()));
        //! ---------------------------------------------------------



        ItemStack previous = node->getMetadata(ProductionNodeProperties::MAIN_BLUEPRINT, QVariant::fromValue(ItemStack())).value<ItemStack>();
        if (previous.isValid() && previous.isBlueprint()) {
            lineEditName->setText(previous.getItem()->getName());
            timeEfficiency->setValue((1. - previous.getBlueprintTimeModifier()) * 100.);
            materialEfficiency->setValue((1. - previous.getBlueprintMaterialsModifier()) * 100.);
            isBPC->setChecked(previous.isBPC());
        }
        Util::println("Designing ends");
    }

    void fetchTotalPrice(ProductionNode* node, Character* character, std::function<void(double)> recv) override {
        if (node == nullptr) recv(0.);
        std::shared_ptr<QMap<ItemStackDatas, int>> work = std::make_shared<QMap<ItemStackDatas, int>>(probeMissingItems(node));

        if (work->isEmpty()) recv(0.);
        else {
            _fetchTotalPriceNode(node, character, 0., recv, work, work->constBegin(), work->constEnd());
        }
    }

    void _fetchTotalPriceNode(ProductionNode* node, Character* character, double current, std::function<void(double)> recv, std::shared_ptr<QMap<ItemStackDatas, int>> map, QMap<ItemStackDatas, int>::const_iterator it, QMap<ItemStackDatas, int>::const_iterator end) {
        if (it == end) {
            recv(current);
            return;
        }

        if (!it.key().isValid()) {
            _fetchTotalPriceNode(node, character, current, recv, map, std::next(it), end);
        } else it.key().getItem()->priceFor(node->getLocation(), it.value(), [this, node, character, current, map, recv, it, end] (double d) mutable {
            if (d == -1) {
                recv(-1);
                return;
            }
            current += d;

            _fetchTotalPriceNode(node, character, current, recv, map, std::next(it), end);
        }, true);
    }

    void designNodeItem(ProductionNodeItem* item) override {
        DefaultProductionNodeType::designNodeItem(item);
        ProductionNode* node = item->getNode();

        QGraphicsProxyWidget* itemLine = new QGraphicsProxyWidget(item);

        QFrame* holder = new QFrame();
        new QVBoxLayout(holder);
        holder->layout()->setContentsMargins(1, 1, 1, 1);
        holder->setFrameShadow(QFrame::Plain);
        holder->setFrameShape(QFrame::Box);
        holder->setObjectName("itemBoxFrame");
        holder->setStyleSheet("QFrame#itemBoxFrame { border: 1px solid white; }");
        holder->setFixedSize(160, 30);

        LinedItemWidgetV2* widget = new LinedItemWidgetV2(holder, true);
        widget->limitIconSize(50);
        widget->setItem(node->getMetadataRef(ProductionNodeProperties::MAIN_BLUEPRINT).value<ItemStack>());
        widget->refresh();
        holder->layout()->addWidget(widget);

        itemLine->setWidget(holder);
        itemLine->setPos(5, 70);
    }

    void insertDBLine(ProductionNode* node) override {
        bool ok;
        ItemStack blueprint = node->getMetadataRef(ProductionNodeProperties::MAIN_BLUEPRINT).value<ItemStack>();
        EsiManager::requestERP(
            "INSERT INTO prodmanufacturenodesdatas (chainLocalId, nodeLocalId, blueprintName, matModifier, timeModifier, bpc, buildingStationLocalId, runCount) "
            "VALUES (:nodeChainId, :nodeLocalId, :blueprintName, :matModifier, :timeModifier, :isbpc, :buildingStationLocalId, :runCount);",
            {
             {"nodeChainId", (node->hasProductionChain() ? node->getProductionChain()->getLocalId() : 0)},
             {"nodeLocalId", node->getLocalId()},
             {"blueprintName", blueprint.isValid() ? blueprint.getItem()->getName() : QString(NULLSTRING_BLUEPRINT)},
             {"matModifier", blueprint.getBlueprintMaterialsModifier()},
             {"timeModifier", blueprint.getBlueprintTimeModifier()},
             {"isbpc", blueprint.isBPC()},
             {"buildingStationLocalId", node->hasLocation() ? node->getLocation()->getLocalId() : 0},
             {"runCount", blueprint.getBlueprintRunCount()}
            }, &ok);
        if (!ok) Util::error("Couldn't insert " + node->getName() + " in spe table");
    }

    QMap<QString, QSet<ItemView>> requiredItems(const ProductionNode* node) override {
        return {};
    }

    int getOutputQuantity(ProductionNode* node, const ItemStackDatas& datas) override {
        ItemStack output = node->getMetadataRef(ProductionNodeProperties::MAIN_BLUEPRINT).value<ItemStack>().getBlueprint()->getOutput();
        if (output.isSimilar(datas)) return output.getQuantity() * node->getRunsCount();
        else return 0;
    }

    int getInputQuantity(ProductionNode* node, const ItemStackDatas& datas) override {
        ItemStack bp = node->getMetadataRef(ProductionNodeProperties::MAIN_BLUEPRINT).value<ItemStack>();
        Blueprint::RecipeMap inputs = bp.getBlueprint()->getRecipeInput();
        return inputs.value(datas, 0) * node->getRunsCount();
    }

    QSet<ItemStack> getOutputs(ProductionNode* node) override {
        return {node->getMetadataRef(ProductionNodeProperties::MAIN_BLUEPRINT).value<ItemStack>().getBlueprint()->getOutput().multipliedQuantity(node->getRunsCount())};
    }
};

class SellNodeType : public DefaultProductionNodeType {
public:
    SellNodeType(int id, const QString &name, const QString& tableName, const QSet<ProductionNodeProperty *> &properties, const QColor &color)
        : DefaultProductionNodeType(id, name, tableName, properties, color) {
        addProperty(ProductionNodeProperties::LONG_SOLD_INPUTS);
        addProperty(ProductionNodeProperties::IMMEDIATE_SOLD_INPUTS);
    }


    void designEditionView(ProductionNodeEditionWidget* parent, ProductionNode* node) override {
        parent->clearProperties();
    }

    void designNodeItem(ProductionNodeItem* item) override {
        DefaultProductionNodeType::designNodeItem(item);
    }

    void insertDBLine(ProductionNode* node) override {

    }

    QMap<QString, QSet<ItemView>> requiredItems(const ProductionNode* node) override {
        return {};
    }

    int getOutputQuantity(ProductionNode* node, const ItemStackDatas& datas) override {
        return 0;
    }

    int getInputQuantity(ProductionNode* node, const ItemStackDatas& datas) override {
        return -1;
    }

    QSet<ItemStack> getOutputs(ProductionNode* node) override {
        return {};
    }
};

ProductionNodeType* ProductionNodeTypes::EMPTY_NODE = new DefaultProductionNodeType(0, "Empty", "prodtrashbin", {}, QColor(255, 255, 255));
ProductionNodeType* ProductionNodeTypes::SELL = new SellNodeType(1, "Sell", "prodtrashbin", {}, QColor(0, 200, 0));
ProductionNodeType* ProductionNodeTypes::MANUFACTURE = new ManufactureNodeType(2, "Manufacture", "prodmanufacturenodesdatas", {}, QColor(247, 180, 48));
ProductionNodeType* ProductionNodeTypes::TIME_RESEARCH = new DefaultProductionNodeType(3, "Time Research", "prodtrashbin", {}, QColor(48, 247, 182));
ProductionNodeType* ProductionNodeTypes::MATERIAL_RESEARCH = new DefaultProductionNodeType(4, "Material Research", "prodtrashbin", {}, QColor(48, 247, 255));
ProductionNodeType* ProductionNodeTypes::COPY = new SingleOutputProductionNodeType(5, "Copy", "prodtrashbin", {}, QColor(199, 75, 155));
ProductionNodeType* ProductionNodeTypes::INVENTION = new SingleOutputProductionNodeType(6, "Invention", "prodtrashbin", {}, QColor(0, 113, 233));
ProductionNodeType* ProductionNodeTypes::HAULING = new DefaultProductionNodeType(7, "Hauling", "prodtrashbin", {}, QColor(0, 156, 114));

QList<ProductionNodeType*> ProductionNodeTypes::VALUES = { // Construit comme ça car on veut garantir l'ordre des ids
    EMPTY_NODE, SELL, MANUFACTURE, TIME_RESEARCH, MATERIAL_RESEARCH, COPY, INVENTION, HAULING
};

void ProductionNode::designEditionWidget(ProductionNodeEditionWidget* widget) {
    getType()->designEditionView(widget, this);
}

ProductionNode* ProductionNode::addParent(ProductionNode* parent) {
    if (!hasProductionChain()) {
        Util::error("Adding parent to a node outside of a chain");
        throw new std::exception;
    }
    getProductionChain()->addParentProductionNode(parent, this);
    parent->setProductionChain(getProductionChain());
    return this;
}

void ProductionNode::setProductionChain(ProductionChain* chain) {
    _setProductionChain(chain);
    if (!chain->getAllNodes().contains(getLocalId()))
        chain->addRawProductionNode(this);
}

ProductionNode* ProductionNode::addChild(ProductionNode* child) {
    if (!hasProductionChain()) {
        Util::error("Adding child to a node outside of a chain");
        throw new std::exception;
    }
    getProductionChain()->addChilProductionNode(this, child);
    child->setProductionChain(getProductionChain());
    return this;
}

void ProductionNode::_addParent(ProductionNode* parent) {
    parent->_addChild(this);
}
void ProductionNode::_addChild(ProductionNode* child) {
    if (!hasChild(child)) children.append(NodeConnection{this, child});
    if (!child->hasParent(this)) child->parents.insert(NodeConnection{this, child});
}

void ProductionNode::removeChild(ProductionNode* child) {
    this->children.removeAll(NodeConnection{this, child});
    if (child->hasParent(this)) child->removeParent(this);
}

void ProductionNode::removeParent(ProductionNode* parent) {
    this->parents.remove(NodeConnection{parent, this});
    parent->removeChild(this);
}

ProductionNodeType::ProductionNodeType(int id, const QString &name, const QString &tableName, const QSet<ProductionNodeProperty *> &properties)
    : id(id), name(name), saveTableName(tableName), properties(properties)
{
}

void ProductionChain::saveToDB() {
    Util::println("Starting to save");
    bool ok = false;
    EsiManager::ERP.transaction();
    QSqlQuery query = EsiManager::requestERP(
        "INSERT INTO prodchains(localId, name, description, idNodesCounter) "
        "VALUES(:localId, :name, :description, :idNodesCounter) "
        "ON CONFLICT(localId) "
        "DO UPDATE SET "
        "name = excluded.name, "
        "description = excluded.description, "
        "idNodesCounter = excluded.idNodesCounter;",
        {{"localId", getLocalId()}, {"name", getName()},
         {"description", getDescription()}, {"idNodesCounter", nodesIdCounter}}, &ok);

    if (!ok) {
        Util::error("Can't save prod chain at first step.");
    } else {
        EsiManager::requestERP("DELETE FROM prodnodeschildren WHERE chainLocalId = :localId", {{"localId", getLocalId()}});
        for (ProductionNodeType* type: ProductionNodeTypes::VALUES) {
            bool temp = true;
            EsiManager::requestERP("DELETE FROM prodnodes WHERE chainLocalId = :chainLocalId", {{"chainLocalId", getLocalId()}}, &temp);
            if (temp) Util::error("Can't clean up node for " + type->getName());
            EsiManager::requestERP("DELETE FROM " + type->getTableName() + " WHERE chainLocalId = :chainLocalId", {{"chainLocalId", getLocalId()}}, &temp);
            if (temp) Util::error("Can't clean up node from type's table for " + type->getName());
        }
        QSet<ProductionNode*> errors = {};
        for (ProductionNode* node: allNodes) {
            QSqlQuery query = EsiManager::requestERP(
                "INSERT INTO prodnodes(chainLocalId, nodeLocalId, type) "
                "VALUES(:chainLocalId, :nodeLocalId, :type) "
                "ON CONFLICT(chainLocalId, nodeLocalId) "
                "DO UPDATE SET "
                "type = excluded.type;",
                {{"chainLocalId", getLocalId()}, {"nodeLocalId", node->getLocalId()}, {"type", node->getType()->getId()}}, &ok);

            if (!ok) {
                Util::error("Unable to save " + node->getName() + " in prodnodes");
                errors.insert(node);
            } else {
                for (const NodeConnection& conn: node->getChildren()) {
                    EsiManager::requestERP(
                        "INSERT INTO prodnodeschildren (parentLocalId, chainLocalId, childLocalId) "
                        "VALUES (:parentId, :chainId, :childId)",
                        {{"parentId", node->getLocalId()}, {"chainId", getLocalId()}, {"childId", conn.getChild()->getLocalId()}}, &ok);
                    if (!ok) Util::error("Unable to add child " + conn.getChild()->getName() + " to " + node->getName());
                }

                node->getType()->insertDBLine(node);
            }
        }

        EsiManager::requestERP("DELETE FROM prodchainfirstnodes WHERE chainLocalId = :chainId", {{"chainId", getLocalId()}}, &ok);
        if (!ok) Util::error("Unable to clear first nodes");
        for (ProductionNode* entry: inputs) {
            EsiManager::requestERP(
                "INSERT INTO prodchainfirstnodes (chainLocalId, nodeLocalId) VALUES (:chainId, :nodeId)",
                {{"chainId", getLocalId()}, {"nodeId", entry->getLocalId()}});
        }
    }
    EsiManager::ERP.commit();
    Util::println("Done.");
}

QMap<QString, QList<ItemStack>> ProductionNode::probeReceivedItems() {
    return {};
}

void ProductionChain::refreshNodeIODatasMap() {
    nodeIODatas.clear();
    QSet<ProductionNode*> loaded = {};
    QSet<ProductionNode*> toLoad = inputs;

    bool cont = true;
    while (!toLoad.isEmpty()) {
        for (ProductionNode* node: toLoad) {
            if (!loaded.contains(node)) {
                refreshNodeIODatas(node);
            }

            loaded.insert(node);
        }

        QSet<ProductionNode*> nextLoad = {};
        for (ProductionNode* node: toLoad) {
            for (const NodeConnection& conn: node->getChildren()) {
                if (loaded.contains(conn.getChild())) continue;
                nextLoad.insert(conn.getChild());
            }
        }

        toLoad = nextLoad;
    }

    for (const auto& [node, map1]: nodeIODatas.asKeyValueRange()) {
        for (const auto& [localId, map2]: map1.outputs.asKeyValueRange()) {
            for (const auto& [key, list]: map2.asKeyValueRange()) {

                QMap<ItemStackDatas, int> refs = {};
                QList<ItemStack> w = {};
                w.reserve(list.size());
                for (const ItemStack& item: list) {
                    int index = refs.value(item.getDatas(), -1);
                    if (index != -1) {
                        w[index].setQuantity(w[index].getQuantity() + item.getQuantity());
                    } else {
                        w.append(item);
                        refs[item.getDatas()] = w.length() - 1;
                    }
                }

                nodeIODatas[node].outputs[localId][key] = w;
            }
        }
    }

    setIOUpdated(false);

    Util::println(nodeIODatas.toString());
    Util::println("------------------------------------------------------------");
}

void ProductionChain::refreshNodeIODatas(ProductionNode* node) {
    ProductionNodeIODatas work = {};

    QQueue<ItemStreamSlot> hierarchy;
    for (const ItemStackDatas& item: node->getItemStream().getTransittingItems()) {
        if (!item.isValid()) continue;
        hierarchy = node->getItemStream().getHierarchy(item);
        int outputAmount = node->getOutputAmount(item);
        for (ItemStreamSlot pair: hierarchy) {
            ProductionNode* node = get(pair.nodeLocalId);
            int inputAmount = node->getRequiredAmount(item);
            ItemStack res = ItemStack{item, inputAmount <= 0 ? outputAmount : std::min(inputAmount, outputAmount)};
            work.outputs[pair.nodeLocalId][pair.port].append(res);
        }
    }
}

Character* ProductionNode::getCharacter() const { return getProductionChain()->getCharacter(); }

NodeConnection NodeConnection::NULLCONN = NodeConnection();

ProductionNode *NodeConnection::getParent() const
{
    return parent;
}

ProductionNode *NodeConnection::getChild() const
{
    return child;
}

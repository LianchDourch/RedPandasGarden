#ifndef PRODUCTIONNODES_H
#define PRODUCTIONNODES_H

#include <QString>
#include "util.h"
#include "core.h"

#define GENERAL_INPUT_PNKEY "input_generic"
#define OUTPUTS_PNEW_KEY "prodedition_outputs"
#define NULLSTRING_BLUEPRINT "$NoBlueprint"

class ProductionNodeItem;
class ProductionNode;
class ProductionNodeEditionWidget;

using NODE_OUTPUT_TABLE = QMap<int, QMap<QString, QList<ItemStack>>>;
Q_DECLARE_METATYPE(NODE_OUTPUT_TABLE);



class GraphicsTextItemObject : public QGraphicsObject
{
    Q_OBJECT

private:
    QGraphicsTextItem* deltaZ = nullptr;

public:
    GraphicsTextItemObject(QGraphicsItem *parent)
        : QGraphicsObject(parent)
    {
        deltaZ = new QGraphicsTextItem(this);
    }

    void setPlainText(const QString &str) {
        this->deltaZ->setPlainText(str);
    }

    QRectF boundingRect() const override
    {
        return deltaZ ? deltaZ->boundingRect() : QRectF();
    }

    void paint(QPainter*,
               const QStyleOptionGraphicsItem*,
               QWidget*) override
    {

    }
};

class ProductionNodeProperty {
public:
    using PropertyProviderMap = QMap<QString, ProductionNodeProperty*>;
private:
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, propertyKey, getPropertyKey);
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, propertyName, getPropertyName);
    std::function<QWidget*(QWidget* parent, std::function<void(QVariant)> receiver, ERROR_LISTENER_CPP)> providerBuilder;

public:
    CONSTRUCTOR(ProductionNodeProperty, propertyKey, propertyName) {}

    ProductionNodeProperty* setProvider(std::function<QWidget*(QWidget* parent, std::function<void(QVariant)> receiver, ERROR_LISTENER_CPP)> providerBuilder) { providerBuilder = (providerBuilder); return this; }

    QWidget* getProvider(QWidget* parent, std::function<void(QVariant)> receiver, ERROR_LISTENER) { return providerBuilder(parent, receiver, errorListener); }
};

struct ProductionNodeProperties {
    inline static ProductionNodeProperty* MAIN_BLUEPRINT = new ProductionNodeProperty("mainblueprint", "Main Blueprint");
    inline static ProductionNodeProperty* MATERIALS_INPUTS = new ProductionNodeProperty("matinputs", "Material Inputs");
    inline static ProductionNodeProperty* LONG_SOLD_INPUTS = new ProductionNodeProperty("sellorder_soldinputs", "Sold via Sell Orders");
    inline static ProductionNodeProperty* IMMEDIATE_SOLD_INPUTS = new ProductionNodeProperty("buyorder_soldinputs", "Sold via Buy Orders");

    inline static QMap<QString, ProductionNodeProperty*> PROPERTIES = {
        {MAIN_BLUEPRINT->getPropertyKey(), MAIN_BLUEPRINT},
        {MATERIALS_INPUTS->getPropertyKey(), MATERIALS_INPUTS},
        {LONG_SOLD_INPUTS->getPropertyKey(), LONG_SOLD_INPUTS},
        {IMMEDIATE_SOLD_INPUTS->getPropertyKey(), IMMEDIATE_SOLD_INPUTS}
    };

    inline static ProductionNodeProperty* fromKey(const QString& key) { return PROPERTIES.value(key, nullptr); }
    inline static ProductionNodeProperty* fromName(const QString& name) {
        for (const auto& [k, v]: PROPERTIES.asKeyValueRange()) {
            if (v->getPropertyName() == name) return v;
        }
        return nullptr;
    }
};

class ProductionNodeType {
private:
    CONST_CONSTRUCTABLE_PROPERTY_POD(int, id, getId);
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, name, getName);
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QSet<ProductionNodeProperty*>, properties, getProperties)
    CONST_CONSTRUCTABLE_PROPERTY_BIGPOD(QString, saveTableName, getTableName)
public:
    ProductionNodeType(int id, const QString &name, const QString& tableName, const QSet<ProductionNodeProperty*> &properties);

    virtual void insertDBLine(ProductionNode* node) = 0;
    virtual void loadDBLine(ProductionNode* node) = 0;
    virtual void designNodeItem(ProductionNodeItem*) = 0;
    virtual void designEditionView(ProductionNodeEditionWidget* parent, ProductionNode* node) = 0;
    virtual QSizeF getNodeSize(const ProductionNode* node) = 0;
    virtual QColor getNodeColor(const ProductionNode* node) = 0;
    virtual QString getNodeLabel(const ProductionNode* node) = 0;
    virtual QMap<QString, QSet<ItemView>> requiredItems(const ProductionNode* node) = 0;
    virtual int getOutputQuantity(ProductionNode* node, const ItemStackDatas& datas) = 0;
    virtual int getInputQuantity(ProductionNode* node, const ItemStackDatas& datas) = 0;
    virtual QSet<ItemStack> getOutputs(ProductionNode* node) = 0;
    virtual void fetchTotalPrice(ProductionNode* node, Character* character, std::function<void(double)> recv) = 0;

    QMap<QString, QSet<ItemView>> getRequiredItems(ProductionNode* node);

protected:
    inline void addProperty(ProductionNodeProperty* prop) { this->properties.insert(prop); }
};

struct ProductionNodeTypes {
    static ProductionNodeType* EMPTY_NODE;
    static ProductionNodeType* SELL;
    static ProductionNodeType* MANUFACTURE;
    static ProductionNodeType* TIME_RESEARCH;
    static ProductionNodeType* MATERIAL_RESEARCH;
    static ProductionNodeType* COPY;
    static ProductionNodeType* INVENTION;
    static ProductionNodeType* HAULING;

    static QList<ProductionNodeType*> VALUES;

    inline static ProductionNodeType* fromName(const QString &name) {
         for (ProductionNodeType* type: VALUES) if (type->getName() == name) return type;
        return nullptr;
    }
};

struct ItemFilter {
    std::function<bool(const ItemStack&)> executor;

    ItemFilter(const std::function<bool(const ItemStack&)>& e) : executor(e) {}
    ItemFilter(const ItemView& view) : executor([view] (const ItemStack& i) { return view.accept(i); }) {}

    inline bool accepts(const ItemStack& item) const { return executor(item); }
};

struct ItemStreamSlot {
    int nodeLocalId = 0;
    ProductionNodeProperty* port = nullptr;

    inline QString getName(const QString &nodeName) const {
        return nodeName + "#" + QString::number(nodeLocalId) + QString(" [%1]").arg(port == nullptr ? "" : port->getPropertyName());
    }
    inline QString getName() const {
        return getName("Node");
    }

    inline bool isNull() const { return port == nullptr || nodeLocalId == 0; }

    inline static ItemStreamSlot fromName(const QString& name) {
        int index = name.lastIndexOf("#");
        if (index == -1) return ItemStreamSlot{};

        QStringList work = name.mid(index + 1).split(" ");
        int nodeId = work[0].toInt();
        ProductionNodeProperty* property = ProductionNodeProperties::fromName(work[1].mid(1, work[1].length() - 2));
        return {nodeId, property};
    }

    inline bool operator==(const ItemStreamSlot& o) const {
        return nodeLocalId == o.nodeLocalId && port == o.port;
    }
};

/**
 * @brief The ItemStream class
 */
class ItemStream {
private:
    /**
     * @brief repartition
     * Pour chaque item produit, l'ordre de priorité de répartition, c'est à dire par exemple pour l'élément associé à "Talwar", on essaie d'abord de remplir les demandes de la première node, dans la case du QString, puis celles de la seconde, etc.
     */
    QMap<ItemStackDatas, QQueue<ItemStreamSlot>> repartition = {};

public:
    ItemStream() {}

    inline QList<ItemStackDatas> getTransittingItems() const { return repartition.keys(); }
    inline const QQueue<ItemStreamSlot> getHierarchy(const ItemStackDatas& datas) const { return repartition[datas]; }
    inline void connectReceiver(const ItemStackDatas& key, ItemStreamSlot recv) { repartition[key].enqueue(recv); }

    inline void setHierarchy(const ItemStackDatas& key, const QQueue<ItemStreamSlot>& value) {
        repartition[key] = value;
    }
};

class NodeConnection {
private:
    ProductionNode* parent;
    ProductionNode* child;
public:
    NodeConnection() : NodeConnection(nullptr, nullptr) {}
    NodeConnection(ProductionNode* parent, ProductionNode* child) : parent(parent), child(child) {}
    ~NodeConnection() { }

    ProductionNode *getParent() const;
    ProductionNode *getChild() const;

    inline bool isNull() const { return parent == nullptr || child == nullptr; }

    /**
     * @brief operator ==
     * @param lhs
     * @param rhs
     * @return true if the parent and child are the same, doesn't check the metadatas !!!
     */
    friend bool operator==(const NodeConnection& lhs, const NodeConnection& rhs) noexcept
    {
        return lhs.parent == rhs.parent
               && lhs.child == rhs.child;
    }

public:
    static NodeConnection NULLCONN;
};


inline size_t qHash(const NodeConnection& c,
                    size_t seed = 0) noexcept
{
    return qHashMulti(seed,
                      c.getParent(),
                      c.getChild());
}

class ProductionNode {
    using NodeMeta = QHash<ProductionNodeProperty*, QVariant>;
private:
    GENERAL_PROPERTY_POD(quint64, localId, 0, getLocalId, setLocalId)
    GENERAL_PROPERTY_PTR(ProductionNodeType*, type, nullptr, getType, setType);
    FULL_PROPERTY_PTR(Station*, location, nullptr, getLocation, setLocation, hasLocation);
    CONST_PROPERTY_BIGPOD(NodeMeta, metadatas, getMetadatas);
    CONST_PROPERTY_BIGPOD(QSet<NodeConnection>, parents, getParents);
    CONST_PROPERTY_BIGPOD(QList<NodeConnection>, children, getChildren);
    FULL_PROPERTY_PTR(ProductionChain*, chain, nullptr, getProductionChain, _setProductionChain, hasProductionChain);
    CONST_PROPERTY_BIGPOD(QString, description, getDescription)
    GENERAL_PROPERTY_POD(int, cycleCount, 1, getRunsCount, setCycleCount)
    GENERAL_PROPERTY_BIGPOD(ItemStream, stream, ItemStream(), getItemStream, setItemStream)

public:
    ProductionNode(ProductionNodeType* type, Station* location)
        : type(type), location(location), metadatas({}), parents({}), children({}), description(QString()), stream() {
    }

    ~ProductionNode() {
        if (!parents.isEmpty() || !children.isEmpty()) {
            Util::error("Destroying node but not connections");
        }
    }

    inline QVariant& getMetadataRef(ProductionNodeProperty* prop) { return metadatas[prop]; }
    inline QVariant getMetadata(ProductionNodeProperty* prop, QVariant defaultValue) { return metadatas.value(prop, defaultValue); }
    inline void setMetadata(ProductionNodeProperty* prop, QVariant value) { this->metadatas[prop] = value; }

    inline bool hasParent(ProductionNode* parent) { return parents.contains(NodeConnection{parent, this}); }
    inline bool hasChild(ProductionNode* child) { return children.contains(NodeConnection{this, child}); }
    inline bool hasChildren() const { return !children.isEmpty(); }
    inline bool hasParents() const { return !parents.isEmpty(); }

    QMap<QString, QList<ItemStack>> probeReceivedItems();
    QMap<int, QMap<QString, ItemStack>> probeOutputtingItems();

    NodeConnection getChildConnection(ProductionNode* child, NodeConnection defaultValue = NodeConnection()) {
        return getChildConnectionRef(child, defaultValue);
    }

    NodeConnection& getChildConnectionRef(ProductionNode* child, NodeConnection& defaultValue = NodeConnection::NULLCONN) {
        for (NodeConnection& conn: children) if (conn.getChild() == child) return conn;
        return defaultValue;
    }

    /**
     * @brief addParent
     * @param parent
     * @return
     */
    ProductionNode* addParent(ProductionNode* parent);
    ProductionNode* addChild(ProductionNode* child);

    void removeChild(ProductionNode* child);
    void removeParent(ProductionNode* parent);

    void fetchTotalNetCost(Character* character, std::function<void(double)> recv) { return getType()->fetchTotalPrice(this, character, recv); } // Le prix de faire cette action (donc si on fait un item, on ne compte que les couts, car l'item est fabriqué in fine)

    void setProductionChain(ProductionChain* chain);

    inline QString getUniqueName() const { return getName() + QString(" (#%1)").arg(QString::number(getLocalId())); }
    inline static int getLocalIdFromUniqueName(const QString& str) { return str.mid(str.lastIndexOf("#") + 1).replace(")", "").toInt(); }
    inline QColor getColor() const { return getType()->getNodeColor(this); }
    inline QSizeF getViewSize() const { return getType()->getNodeSize(this); }
    inline void design(ProductionNodeItem* item) const { getType()->designNodeItem(item); }
    void designEditionWidget(ProductionNodeEditionWidget* widget);
    inline QString getName() const { return getType()->getNodeLabel(this); }
    inline int getOutputAmount(const ItemStackDatas &datas) { return getType()->getOutputQuantity(this, datas); }
    inline int getRequiredAmount(const ItemStackDatas& datas) { return getType()->getInputQuantity(this, datas); }
    inline QSet<ItemStack> getOutputs() { return getType()->getOutputs(this); }
    inline const QSet<ProductionNodeProperty*>&  getProperties() const { return getType()->getProperties(); }
    inline bool hasProperties() const { return !getProperties().isEmpty(); }
    inline ProductionNodeProperty* getAnyProperty() const { return hasProperties() ? *getProperties().constBegin() : nullptr; }
    Character* getCharacter() const;


    inline ItemStream* getItemStreamPtr() { return &stream; }
private:
    friend ProductionChain;
    void _addParent(ProductionNode* parent);
    void _addChild(ProductionNode* child);
};

struct ProductionNodeIODatas {
    QMap<int, QMap<ProductionNodeProperty*, QList<ItemStack>>> outputs;
};

class NodeIODatasMap : public QMap<int, ProductionNodeIODatas> {
private:

public:
    using QMap<int, ProductionNodeIODatas>::QMap;

    inline ProductionNodeIODatas getOutputs(ProductionNode* node) const {
        return (*this)[node->getLocalId()];
    }

    inline ProductionNodeIODatas& getOutputsRef(ProductionNode* node) {
        return (*this)[node->getLocalId()];
    }

    inline QMap<ProductionNodeProperty*, QList<ItemStack>> getUnsortedInputs(ProductionNode* node) const {
        if (node == nullptr) return {};
        QMap<ProductionNodeProperty*, QList<ItemStack>> res = {};

        for (const NodeConnection& conn : node->getParents()) {
            const ProductionNodeIODatas& view = getOutputs(conn.getParent());
            if (!view.outputs.contains(node->getLocalId())) continue;
            for (const auto& [prop, items] : view.outputs[node->getLocalId()].asKeyValueRange()) {
                res[prop].append(items);
            }
        }

        return res;
    }

    inline QString toString() {
        QString res = "IODatasMap:";
        for (const auto& [k, v]: asKeyValueRange()) {
            res += "\n";
            res += QString::number(k) + ":";
            if (v.outputs.isEmpty()) res += " empty";
            for (const auto& [k2, v2]: v.outputs.asKeyValueRange()) {
                for (const auto& [k3, v3]: v2.asKeyValueRange()) {
                    res += "\n\t";
                    res += "to " + QString::number(k2) + " #" + k3->getPropertyName() + ":";
                    if (v3.isEmpty()) res += " <empty>";
                    for (const ItemStack& item: v3) {
                        res += "\n\t\t - " + item.getItem()->getName() + " x " + QString::number(item.getAmount());
                    }
                }
            }
        }
        return res;
    }
};

class ProductionChain {
    using NodesMap = QMap<int, ProductionNode*>;
private:
    LOCKABLE
    CONST_PROPERTY_POD(int, localId, getLocalId);
    CONST_PROPERTY_BIGPOD(QString, name, getName);
    CONST_PROPERTY_BIGPOD(QSet<ProductionNode*>, inputs, getInputs);
    CONST_PROPERTY_BIGPOD(QSet<ProductionNode*>, outputs, getOutputs);
    CONST_PROPERTY_BIGPOD(NodesMap, allNodes, getAllNodes);
    CONST_PROPERTY_BIGPOD(QString, description, getDescription);
    FULL_PROPERTY_PTR(Character*, character, nullptr, getCharacter, setCharacter, hasCharacter)
    quint64 nodesIdCounter = 1;
    CONST_PROPERTY_BIGPOD(NodeIODatasMap, nodeIODatas, getNodeIODatas);
    std::atomic_bool nodeIOUpdated;

public:
    ProductionChain(int localId, const QString& name, const QString& description)
        : localId(localId), name(name), description(description) {
        inputs.reserve(5);
        outputs.reserve(5);
        nodeIOUpdated.store(true);
    }
    ~ProductionChain() {
        Util::println("Killing chain :(");
    }

    inline void updateNodeIODatasMap() { if (isIOUpdated()) refreshNodeIODatasMap(); }
    void refreshNodeIODatasMap();
    void refreshNodeIODatas(ProductionNode* node);

    void addRawProductionNode(ProductionNode* node, bool isStart, bool isEnd) {
        if (node == nullptr) return;
        if (allNodes.value(node->getLocalId(), nullptr) == node) {
            Util::error("AddRawProductionNode for an already existing node !!!");
            return;
        }
        node->setLocalId(nodesIdCounter++);
        allNodes.insert(node->getLocalId(), node);

        if (isStart) inputs.insert(node);
        if (isEnd) outputs.insert(node);

        if (node->getProductionChain() != this) node->setProductionChain(this);
    }

    inline ProductionNode* get(quint64 localId) {
        return allNodes.value(localId, nullptr);
    }

    void addRawProductionNode(ProductionNode* node) {
        addRawProductionNode(node, !node->hasParents(), !node->hasChildren());
    }

    void addChilProductionNode(ProductionNode* node, ProductionNode* parent) {
        addRawProductionNode(node, false, !node->hasChildren());
        if (outputs.contains(parent)) outputs.remove(parent);
        node->_addParent(parent);
    }

    void addParentProductionNode(ProductionNode* node, ProductionNode* child) {
        addRawProductionNode(node, !node->hasParents(), false);
        if (inputs.contains(child)) {
            inputs.remove(child);
        }
        node->_addChild(child);
    }

    void removeProductionNode(ProductionNode* node, bool deleteIt = true) {
        for (const NodeConnection& conn: node->getParents()) {
            conn.getParent()->removeChild(node);
            if (!conn.getParent()->hasChildren()) outputs.insert(conn.getParent());
        }
        for (const NodeConnection& conn: node->getChildren()) {
            conn.getChild()->removeParent(node);
            if (!conn.getChild()->hasParents()) inputs.insert(conn.getChild());
        }

        inputs.remove(node);
        outputs.remove(node);
        allNodes.remove(node->getLocalId());

        if (deleteIt) delete node;
    }

    void saveToDB();

    inline bool isIOUpdated() const { return nodeIOUpdated.load(); }
    inline void setIOUpdated(bool updated) { nodeIOUpdated.store(updated); }
    inline void notifyIOUpdate() { setIOUpdated(true); }
};

#endif // PRODUCTIONNODES_H

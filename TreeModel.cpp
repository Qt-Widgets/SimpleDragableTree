#include "TreeModel.h"
#include "TreeNode.h"
#include <QMimeData>
#include <QByteArray>
#include <QStack>

static constexpr char mimeType[] = "MyNode";

using namespace std;

TreeModel::TreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , rootNode_{std::make_shared<TreeNode>("ROOT_NODE")}
{
    fillTreeWithData();
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
{
    TreeNode *parentNode = parent.isValid() ?
                static_cast<TreeNode *>(parent.internalPointer()) :
                rootNode_.get();

    return createIndex(row, column, parentNode->child(row).get());
}

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()){
        return {};
    }

    TreeNode *node = static_cast<TreeNode *>(index.internalPointer());
    TreeNode::ParentPtr parentNode = node->parent();

    if (parentNode.expired()){
        return {};
    }

    std::shared_ptr<TreeNode> lockedParentNode = parentNode.lock();
    return createIndex(lockedParentNode->row(), 0, lockedParentNode.get());
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()){
        return rootNode_->childrenCount();
    }

    TreeNode *parentNode = static_cast<TreeNode *>(parent.internalPointer());
    return parentNode->childrenCount();
}

int TreeModel::columnCount(const QModelIndex &/*parent*/) const
{
    return 1;
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() ||
            role != Qt::DisplayRole) {
        return QVariant();
    }

    return static_cast<TreeNode *>(index.internalPointer())->name;
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()){
        return {};
    }

    Qt::ItemFlags result = Qt::ItemIsEnabled |
            Qt::ItemIsSelectable;

    if (!index.parent().isValid()){ // Group nodes cann accept drop
        result |= Qt::ItemIsDropEnabled;
    }
    else{   //leaves can be draged
        result |= Qt::ItemIsDragEnabled;
    }

    return result;
}

Qt::DropActions TreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList TreeModel::mimeTypes() const
{
    return {mimeType};
}

QMimeData *TreeModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *result = new QMimeData();
    result->setData(mimeType, saveIndexes(indexes));
    return result;
}

bool TreeModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int /*column*/, const QModelIndex &parent) const
{
    if (!parent.isValid() || // root is not drop enabled
            !data->hasFormat(mimeType) ||
            row == -1 ||
            action != Qt::MoveAction){
        return false;
    }

    return true;
}

void TreeModel::fillTreeWithData()
{
    const std::vector<TreeNode::ChildPtr> groups = {
        make_shared<TreeNode>("Group 0"),
        make_shared<TreeNode>("Group 1"),
        make_shared<TreeNode>("Group 2")
    };

    for (TreeNode::ChildPtr group: groups){
        rootNode_->addChild(group);

        for(int i = 0; i < 5; ++i){
            group->addChild(
                        make_shared<TreeNode>(QStringLiteral("Item %1 of %2").arg(i).arg(group->name))
                        );
        }
    }
}

QByteArray TreeModel::saveIndexes(const QModelIndexList &indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    for (const QModelIndex &index: indexes){
        QModelIndex localIndex = index;
        QStack<int> indexParentStack;
        while (localIndex.isValid()){
            indexParentStack << localIndex.row();
            localIndex = localIndex.parent();
        }

        stream << indexParentStack.size();
        while (!indexParentStack.isEmpty()){
            stream << indexParentStack.pop();
        }
    }
    return result;
}

QModelIndexList TreeModel::restoreIndexes(QByteArray data)
{
    QModelIndexList result;
    QDataStream stream(&data, QIODevice::ReadOnly);

    while(!stream.atEnd()){
        int childDepth = 0;
        stream >> childDepth;

        QModelIndex currentIndex = {};
        for (int i = 0; i < childDepth; ++i){
            int row = 0;
            stream >> row;
            currentIndex = index(row, 0, currentIndex);
        }
        result << currentIndex;
    }

    return result;
}

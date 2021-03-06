#include "TreeModel.h"
#include "TreeNode.h"
#include <QDataStream>
#include <QMimeData>
#include <QByteArray>
#include <QStack>
#include <algorithm>

using namespace std;

namespace {
static constexpr char mimeType[] = "MyNode";

struct MovableChild{
    TreeNode::ChildPtr ptr;
    QModelIndex parentIndex;
};

    QList<MovableChild> convertIndexesToMovableChildren(const QModelIndexList &indexes){
        QList<MovableChild> result;
        result.reserve(indexes.count());

        for (const QModelIndex &index: indexes){
            if (!index.isValid()){
                result.append({nullptr, {}});
                continue;
            }

            TreeNode *child = static_cast<TreeNode *>(index.internalPointer());
            result.append({child->shared_from_this(), index.parent()});
        }

        return result;
    }
}


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
            action != Qt::MoveAction ||
            !data->hasFormat(mimeType) ||
            row < 0 ||
            row > rowCount(parent)){
        return false;
    }

    return true;
}

bool TreeModel::dropMimeData(const QMimeData *data, Qt::DropAction /*action*/, int row, int /*column*/, const QModelIndex &parent)
{
    QModelIndexList indexes = restoreIndexes(data->data(mimeType));
    if (indexes.isEmpty()){
        return false;
    }

    sortIndexes(indexes);
    const QList<MovableChild> &childrenToMove = convertIndexesToMovableChildren(indexes);

    TreeNode *parentNode = static_cast<TreeNode *>(parent.internalPointer());
    for (const MovableChild &movableChild: childrenToMove){
        const int srcRow = movableChild.ptr->row();
        const bool interParentMove = movableChild.parentIndex == parent;
        const bool incrementRow = !(interParentMove && srcRow < row);

        beginMoveRows(movableChild.parentIndex, srcRow, srcRow, parent, row);
        parentNode->insertChild(movableChild.ptr, row);
        endMoveRows();

        row += incrementRow;
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
        rootNode_->insertChild(group);

        for(int i = 0; i < 5; ++i){
            group->insertChild(
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

void TreeModel::sortIndexes(QModelIndexList &indexes)
{
    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex &left, const QModelIndex &right){
        if (left.parent() < right.parent()){
            return true;
        }
        else if (right.parent() < left.parent()){
            return false;
        }
        else{
            return left < right;
        }
    });
}

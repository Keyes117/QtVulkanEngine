#include "QuadTree.h"

QuadTree::QuadTree(const AABB& bounds) :
    m_root(std::make_unique<Node>(bounds, 0))
{
}

void QuadTree::insert(const std::shared_ptr<Object>& object)
{
    if (object && object->getModel())
        insertObject(m_root.get(), object);
}

void QuadTree::remove(const std::shared_ptr<Object>& object)
{
    if (object)
        removeObject(m_root.get(), object);
}

void QuadTree::update(const std::shared_ptr<Object>& object)
{
    if (object)
    {
        remove(object);
        insert(object);
    }
}

std::vector<std::shared_ptr<Object>> QuadTree::query(const AABB& bounds)
{
    std::vector<std::shared_ptr<Object>> result;
    queryNode(m_root.get(), bounds, result);
    return result;
}

void QuadTree::clear()
{
    m_root = std::make_unique<Node>(m_root->bounds, 0);
}

void QuadTree::insertObject(Node* node, const std::shared_ptr<Object>& object)
{
    if (!node->bounds.overlaps(object->getBoundingBox()))
        return;

    if (node->isLeaf())
    {
        node->objects.push_back(object);
        if (node->objects.size() > MAX_OBJECT_PER_NODE && node->level < MAX_DEPTH)
            node->split();
    }
    else
    {
        for (auto& child : node->children)
        {
            insertObject(child.get(), object);
        }
    }
}

void QuadTree::removeObject(Node* node, const std::shared_ptr<Object>& object)
{
    if (!node)
        return;

    auto iter = std::find(node->objects.begin(), node->objects.end(), object);
    if (iter != node->objects.end())
    {
        node->objects.erase(iter);
    }

    if (!node->isLeaf())
    {
        for (auto& child : node->children)
        {
            removeObject(child.get(), object);
        }
    }
}

void QuadTree::queryNode(Node* node, const AABB& queryBounds, std::vector<std::shared_ptr<Object>>& result)
{
    if (!node || !node->bounds.overlaps(queryBounds))
        return;

    for (auto object : node->objects)
    {
        if (object->getBoundingBox().overlaps(queryBounds))
            result.push_back(object);
    }

    if (!node->isLeaf())
    {
        for (auto& child : node->children)
        {
            queryNode(child.get(), queryBounds, result);
        }
    }
}

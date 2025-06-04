#include "QuadTree.h"

QuadTree::QuadTree(const AABB& bounds) :
    m_root(std::make_unique<Node>(bounds, 0))
{
}

void QuadTree::insert(Object* object)
{
    if (object && object->getModel())
        insertObject(m_root.get(), object);
}

void QuadTree::remove(Object* object)
{
    if (object)
        removeObject(m_root.get(), object);
}

void QuadTree::update(Object* object)
{
    if (object)
    {
        remove(object);
        insert(object);
    }
}

std::vector<Object*> QuadTree::query(const AABB& bounds)
{
    std::vector<Object*> result;
    queryNode(m_root.get(), bounds, result);
    return result;
}

void QuadTree::clear()
{
    m_root = std::make_unique<Node>(m_root->bounds, 0);
}

void QuadTree::insertObject(Node* node, Object* object)
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

void QuadTree::removeObject(Node* node, Object* object)
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

void QuadTree::queryNode(Node* node, const AABB& queryBounds, std::vector<Object*>& result)
{
    if (!node || !node->bounds.overlaps(queryBounds))
        return;

    for (auto* object : node->objects)
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

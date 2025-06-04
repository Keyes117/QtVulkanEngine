#pragma once

#include <array>
#include <vector>
#include <memory>

#include "Object.h"
#include "const.h"


class QuadTree
{
public:
    static constexpr int MAX_DEPTH = 8;
    static constexpr size_t MAX_OBJECT_PER_NODE = 32;

    QuadTree(const AABB& bounds);
    void insert(Object* object);
    void remove(Object* object);
    void update(Object* object);
    std::vector<Object*> query(const AABB& bounds);
    void clear();
private:
    struct Node
    {
        AABB bounds;
        int level;
        std::vector<Object*> objects;
        std::array<std::unique_ptr<Node>, 4> children;

        Node(const AABB& b, int l) : bounds(b), level(l) {}

        bool isLeaf() const {
            return !children[0];
        }

        void split()
        {
            if (level >= MAX_DEPTH)
                return;
            float halfwidth = (bounds.maxX - bounds.minX) * 0.5f;
            float halfheight = (bounds.maxY - bounds.minY) * 0.5f;
            float centerX = bounds.minX + halfwidth;
            float centerY = bounds.minY + halfheight;

            children[0] = std::make_unique<Node>(
                AABB(bounds.minX, centerY, centerX, bounds.maxY), level + 1);
            children[1] = std::make_unique<Node>(
                AABB(centerX, centerY, bounds.maxX, bounds.maxY), level + 1);
            children[2] = std::make_unique<Node>(
                AABB(bounds.minX, bounds.minY, centerX, centerY), level + 1);
            children[3] = std::make_unique<Node>(
                AABB(centerX, bounds.minY, bounds.maxX, centerY), level + 1);

            auto oldObjects = std::move(objects);
            for (auto* object : oldObjects)
            {
                for (auto& child : children)
                {
                    if (object->getBoundingBox().overlaps(child->bounds))
                    {
                        child->objects.push_back(object);
                    }
                }
            }
        }

    };

    std::unique_ptr<Node> m_root;

    void insertObject(Node* node, Object* object);
    void removeObject(Node* node, Object* object);
    void queryNode(Node* node, const AABB& queryBounds, std::vector<Object*>& result);

};


#pragma once

#include <array>
#include <vector>
#include <memory>

#include "Object.h"
#include "const.h"


class QuadTree
{
public:
    static constexpr int MAX_DEPTH = 12;
    static constexpr size_t MAX_OBJECT_PER_NODE = 300;

    QuadTree(const AABB& bounds);
    void insert(const std::shared_ptr<Object>& object);
    void remove(const std::shared_ptr<Object>& object);
    void update(const std::shared_ptr<Object>& object);
    std::vector<std::shared_ptr<Object>> query(const AABB& bounds);
    void clear();
private:
    struct Node
    {
        AABB bounds;
        int level;
        std::vector<std::shared_ptr<Object>> objects;
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
            for (auto object : oldObjects)
            {
                insertToAppropriateChild(object);
            }
        }

        void insertToAppropriateChild(const std::shared_ptr<Object>& object)
        {
            AABB objectBounds = object->getBoundingBox();

            std::vector<int> overlappingChildren;

            for (int i = 0; i < 4; ++i)
            {
                if (children[i] && objectBounds.overlaps(children[i]->bounds))
                {
                    overlappingChildren.push_back(i);
                }
            }
            if (overlappingChildren.size() == 1)
            {
                // 对象只属于一个子节点
                int index = overlappingChildren[0];
                children[index]->objects.push_back(object);

                // 递归检查子节点是否需要进一步分割
                if (children[index]->objects.size() > MAX_OBJECT_PER_NODE &&
                    children[index]->level < MAX_DEPTH)
                    {
                    children[index]->split();
                    }
                }
            else
            {
                // 对象跨越多个子节点，保留在当前节点
                objects.push_back(object);
            }
        }
    };

    std::unique_ptr<Node> m_root;


    void insertObject(Node* node, const std::shared_ptr<Object>& object);
    void removeObject(Node* node, const std::shared_ptr<Object>& object);
    void queryNode(Node* node, const AABB& queryBounds, std::vector<std::shared_ptr<Object>>& result);

};


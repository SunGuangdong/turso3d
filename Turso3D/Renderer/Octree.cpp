// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Math/Ray.h"
#include "Octree.h"

#include <cassert>
#include <algorithm>

static const float DEFAULT_OCTREE_SIZE = 1000.0f;
static const int DEFAULT_OCTREE_LEVELS = 8;
static const int MAX_OCTREE_LEVELS = 256;

bool CompareRaycastResults(const RaycastResult& lhs, const RaycastResult& rhs)
{
    return lhs.distance < rhs.distance;
}

bool CompareNodeDistances(const std::pair<OctreeNode*, float>& lhs, const std::pair<OctreeNode*, float>& rhs)
{
    return lhs.second < rhs.second;
}

Octant::Octant() :
    parent(nullptr),
    numNodes(0),
    sortDirty(false)
{
    for (size_t i = 0; i < NUM_OCTANTS; ++i)
        children[i] = nullptr;
}

void Octant::Initialize(Octant* parent_, const BoundingBox& boundingBox, int level_)
{
    worldBoundingBox = boundingBox;
    center = worldBoundingBox.Center();
    halfSize = worldBoundingBox.HalfSize();
    cullingBox = BoundingBox(worldBoundingBox.min - halfSize, worldBoundingBox.max + halfSize);
    level = level_;
    parent = parent_;
}

bool Octant::FitBoundingBox(const BoundingBox& box, const Vector3& boxSize) const
{
    // If max split level, size always OK, otherwise check that box is at least half size of octant
    if (level <= 1 || boxSize.x >= halfSize.x || boxSize.y >= halfSize.y || boxSize.z >= halfSize.z)
        return true;
    // Also check if the box can not fit inside a child octant's culling box, in that case size OK (must insert here)
    else
    {
        if (box.min.x <= worldBoundingBox.min.x - 0.5f * halfSize.x || box.min.y <= worldBoundingBox.min.y - 0.5f * halfSize.y ||
            box.min.z <= worldBoundingBox.min.z - 0.5f * halfSize.z || box.max.x >= worldBoundingBox.max.x + 0.5f * halfSize.x ||
            box.max.y >= worldBoundingBox.max.y + 0.5f * halfSize.y || box.max.z >= worldBoundingBox.max.z + 0.5f * halfSize.z)
            return true;
    }

    // Bounding box too small, should create a child octant
    return false;
}

Octree::Octree()
{
    root.Initialize(nullptr, BoundingBox(-DEFAULT_OCTREE_SIZE, DEFAULT_OCTREE_SIZE), DEFAULT_OCTREE_LEVELS);
}

Octree::~Octree()
{
    // Clear octree association from nodes that were never inserted
    for (auto it = updateQueue.begin(); it != updateQueue.end(); ++it)
    {
        OctreeNode* node = *it;
        if (node && node->impl->octree == this && !node->impl->octant)
        {
            node->impl->octree = nullptr;
            node->SetFlag(NF_OCTREE_UPDATE_QUEUED, false);
        }
    }
    updateQueue.clear();

    DeleteChildOctants(&root, true);
}

void Octree::RegisterObject()
{
    RegisterFactory<Octree>();
    CopyBaseAttributes<Octree, Node>();
    RegisterRefAttribute("boundingBox", &Octree::BoundingBoxAttr, &Octree::SetBoundingBoxAttr);
    RegisterAttribute("numLevels", &Octree::NumLevelsAttr, &Octree::SetNumLevelsAttr);
}

void Octree::Update(unsigned short frameNumber)
{
    PROFILE(UpdateOctree);

    for (auto it = updateQueue.begin(); it != updateQueue.end(); ++it)
    {
        OctreeNode* node = *it;
        // If node was removed before update could happen, a null pointer will be in its place
        if (!node)
            continue;

        node->SetFlag(NF_OCTREE_UPDATE_QUEUED, false);
        node->lastUpdateFrameNumber = frameNumber;

        // Do nothing if still fits the current octant
        const BoundingBox& box = node->WorldBoundingBox();
        Vector3 boxSize = box.Size();
        Octant* oldOctant = node->impl->octant;
        if (oldOctant && oldOctant->cullingBox.IsInside(box) == INSIDE && oldOctant->FitBoundingBox(box, boxSize))
            continue;

        // Begin reinsert process. Start from root and check what level child needs to be used
        Octant* newOctant = &root;
        Vector3 boxCenter = box.Center();

        for (;;)
        {
            // If node does not fit fully inside root octant, must remain in it
            bool insertHere = (newOctant == &root) ?
                (newOctant->cullingBox.IsInside(box) != INSIDE || newOctant->FitBoundingBox(box, boxSize)) :
                newOctant->FitBoundingBox(box, boxSize);

            if (insertHere)
            {
                if (newOctant != oldOctant)
                {
                    // Add first, then remove, because node count going to zero deletes the octree branch in question
                    AddNode(node, newOctant);
                    if (oldOctant)
                        RemoveNode(node, oldOctant);
                }
                break;
            }
            else
                newOctant = CreateChildOctant(newOctant, newOctant->ChildIndex(boxCenter));
        }
    }

    updateQueue.clear();

    for (auto it = sortDirtyOctants.begin(); it != sortDirtyOctants.end(); ++it)
    {
        Octant* octant = *it;
        std::sort(octant->nodes.begin(), octant->nodes.end());
        octant->sortDirty = false;
    }

    sortDirtyOctants.clear();
}

void Octree::Resize(const BoundingBox& boundingBox, int numLevels)
{
    PROFILE(ResizeOctree);

    // Collect nodes to the root and delete all child octants
    updateQueue.clear();
    CollectNodes(updateQueue, &root);
    DeleteChildOctants(&root, false);
    allocator.Reset();
    root.Initialize(nullptr, boundingBox, Clamp(numLevels, 1, MAX_OCTREE_LEVELS));

    // Nodes will be reinserted on next update
}

void Octree::RemoveNode(OctreeNode* node)
{
    assert(node);
    RemoveNode(node, node->impl->octant);
    if (node->TestFlag(NF_OCTREE_UPDATE_QUEUED))
        CancelUpdate(node);
    node->impl->octant = nullptr;
}

void Octree::QueueUpdate(OctreeNode* node)
{
    assert(node);
    updateQueue.push_back(node);
    node->SetFlag(NF_OCTREE_UPDATE_QUEUED, true);
}

void Octree::CancelUpdate(OctreeNode* node)
{
    assert(node);
    
    for (auto it = updateQueue.begin(); it != updateQueue.end(); ++it)
    {
        if ((*it) == node)
        {
            *it = nullptr;
            break;
        }
    }

    node->SetFlag(NF_OCTREE_UPDATE_QUEUED, false);
}

void Octree::Raycast(std::vector<RaycastResult>& result, const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask)
{
    result.clear();
    CollectNodes(result, &root, ray, nodeFlags, maxDistance, layerMask);
    std::sort(result.begin(), result.end(), CompareRaycastResults);
}

RaycastResult Octree::RaycastSingle(const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask)
{
    // Get first the potential hits
    initialRes.clear();
    CollectNodes(initialRes, &root, ray, nodeFlags, maxDistance, layerMask);
    std::sort(initialRes.begin(), initialRes.end(), CompareNodeDistances);

    // Then perform actual per-node ray tests and early-out when possible
    finalRes.clear();
    float closestHit = M_INFINITY;
    for (auto it = initialRes.begin(); it != initialRes.end(); ++it)
    {
        if (it->second < Min(closestHit, maxDistance))
        {
            size_t oldSize = finalRes.size();
            it->first->OnRaycast(finalRes, ray, maxDistance);
            if (finalRes.size() > oldSize)
                closestHit = Min(closestHit, finalRes.back().distance);
        }
        else
            break;
    }

    if (finalRes.size())
    {
        std::sort(finalRes.begin(), finalRes.end(), CompareRaycastResults);
        return finalRes.front();
    }
    else
    {
        RaycastResult emptyRes;
        emptyRes.position = emptyRes.normal = Vector3::ZERO;
        emptyRes.distance = M_INFINITY;
        emptyRes.node = nullptr;
        emptyRes.subObject = 0;
        return emptyRes;
    }
}

void Octree::SetBoundingBoxAttr(const BoundingBox& boundingBox)
{
    root.worldBoundingBox = boundingBox;
}

const BoundingBox& Octree::BoundingBoxAttr() const
{
    return root.worldBoundingBox;
}

void Octree::SetNumLevelsAttr(int numLevels)
{
    /// Setting the number of level (last attribute) triggers octree resize when deserializing
    Resize(root.worldBoundingBox, numLevels);
}

int Octree::NumLevelsAttr() const
{
    return root.level;
}

void Octree::AddNode(OctreeNode* node, Octant* octant)
{
    octant->nodes.push_back(node);
    node->impl->octant = octant;

    if (!octant->sortDirty)
    {
        octant->sortDirty = true;
        sortDirtyOctants.push_back(octant);
    }

    // Increment the node count in the whole parent branch
    while (octant)
    {
        ++octant->numNodes;
        octant = octant->parent;
    }
}

void Octree::RemoveNode(OctreeNode* node, Octant* octant)
{
    if (!octant)
        return;

    // Do not set the node's octant pointer to zero, as the node may already be added into another octant
    for (auto it = octant->nodes.begin(); it != octant->nodes.end(); ++it)
    {
        if ((*it) == node)
        {
            octant->nodes.erase(it);
            // Decrement the node count in the whole parent branch and erase empty octants as necessary
            while (octant)
            {
                --octant->numNodes;
                Octant* next = octant->parent;
                if (!octant->numNodes && next)
                    DeleteChildOctant(next, next->ChildIndex(octant->center));
                octant = next;
            }
            return;
        }
    }
}

Octant* Octree::CreateChildOctant(Octant* octant, size_t index)
{
    if (octant->children[index])
        return octant->children[index];

    Vector3 newMin = octant->worldBoundingBox.min;
    Vector3 newMax = octant->worldBoundingBox.max;
    const Vector3& oldCenter = octant->center;

    if (index & 1)
        newMin.x = oldCenter.x;
    else
        newMax.x = oldCenter.x;

    if (index & 2)
        newMin.y = oldCenter.y;
    else
        newMax.y = oldCenter.y;

    if (index & 4)
        newMin.z = oldCenter.z;
    else
        newMax.z = oldCenter.z;

    Octant* child = allocator.Allocate();
    child->Initialize(octant, BoundingBox(newMin, newMax), octant->level - 1);
    octant->children[index] = child;

    return child;
}

void Octree::DeleteChildOctant(Octant* octant, size_t index)
{
    allocator.Free(octant->children[index]);
    octant->children[index] = nullptr;
}

void Octree::DeleteChildOctants(Octant* octant, bool deletingOctree)
{
    for (auto it = octant->nodes.begin(); it != octant->nodes.end(); ++it)
    {
        OctreeNode* node = *it;
        node->impl->octant = nullptr;
        node->SetFlag(NF_OCTREE_UPDATE_QUEUED, false);
        if (deletingOctree)
            node->impl->octree = nullptr;
    }
    octant->nodes.clear();
    octant->numNodes = 0;

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
        {
            DeleteChildOctants(octant->children[i], deletingOctree);
            octant->children[i] = nullptr;
        }
    }

    if (octant != &root)
        allocator.Free(octant);
}

void Octree::CollectNodes(std::vector<OctreeNode*>& result, Octant* octant) const
{
    result.insert(result.end(), octant->nodes.begin(), octant->nodes.end());

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i]);
    }
}

void Octree::CollectNodes(std::vector<OctreeNode*>& result, Octant* octant, unsigned short nodeFlags, unsigned layerMask) const
{
    std::vector<OctreeNode*>& octantNodes = octant->nodes;

    for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
    {
        OctreeNode* node = *it;
        if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask))
            result.push_back(node);
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], nodeFlags, layerMask);
    }
}

void Octree::CollectNodes(std::vector<RaycastResult>& result, Octant* octant, const Ray& ray, unsigned short nodeFlags, 
    float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->cullingBox);
    if (octantDist >= maxDistance)
        return;

    std::vector<OctreeNode*>& octantNodes = octant->nodes;

    for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
    {
        OctreeNode* node = *it;
        if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask))
            node->OnRaycast(result, ray, maxDistance);
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], ray, nodeFlags, maxDistance, layerMask);
    }
}

void Octree::CollectNodes(std::vector<std::pair<OctreeNode*, float> >& result, Octant* octant, const Ray& ray, unsigned short nodeFlags,
    float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->cullingBox);
    if (octantDist >= maxDistance)
        return;

    std::vector<OctreeNode*>& octantNodes = octant->nodes;

    for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
    {
        OctreeNode* node = *it;
        if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask))
        {
            float distance = ray.HitDistance(node->WorldBoundingBox());
            if (distance < maxDistance)
                result.push_back(std::make_pair(node, distance));
        }
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], ray, nodeFlags, maxDistance, layerMask);
    }
}

// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/AutoPtr.h"
#include "../Object/Ptr.h"
#include "GraphicsDefs.h"

/// GPU buffer for shader program uniform data. Note: because of poor performance when constantly updating, is used only for special case large data (forward+ lights).
class UniformBuffer : public RefCounted
{
public:
    /// Construct. %Graphics subsystem must have been initialized.
    UniformBuffer();
    /// Destruct.
    ~UniformBuffer();

    /// Define buffer with byte size. Return true on success.
    bool Define(ResourceUsage usage, size_t size, const void* data = nullptr);
    /// Redefine buffer data either completely or partially. Return true on success.
    bool SetData(size_t offset, size_t numBytes, const void* data, bool discard = false);
    /// Bind to use at a specific shader slot. No-op if already bound, unless force is specified.
    void Bind(size_t index, bool force = false);

    /// Return size of buffer in bytes.
    size_t Size() const { return size; }
    /// Return resource usage type.
    ResourceUsage Usage() const { return usage; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return usage == USAGE_DYNAMIC; }

    /// Return the OpenGL object identifier.
    unsigned GLBuffer() const { return buffer; }

    /// Unbind a slot.
    static void Unbind(size_t index);

private:
    /// Create the GPU-side index buffer. Return true on success.
    bool Create(const void* data);
    /// Release the index buffer and CPU shadow data.
    void Release();

    /// OpenGL object identifier.
    unsigned buffer;
    /// Buffer size in bytes.
    size_t size;
    /// Resource usage type.
    ResourceUsage usage;
};

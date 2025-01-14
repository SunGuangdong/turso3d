// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Color.h"
#include "../Math/IntVector2.h"
#include "../Object/Object.h"
#include "GraphicsDefs.h"

struct SDL_Window;

/// %Graphics rendering context and application window.
class Graphics : public Object
{
    OBJECT(Graphics);

public:
    /// Create window with initial size and register subsystem and object. Rendering context is not created yet.
    Graphics(const char* windowTitle, const IntVector2& windowSize);
    /// Destruct. Closes the application window.
    ~Graphics();

    /// Initialize rendering context. Return true on success.
    bool Initialize();
    /// Set new window size.
    void Resize(const IntVector2& size);
    /// Set fullscreen mode.
    void SetFullscreen(bool enable);
    /// Set vertical sync on/off.
    void SetVSync(bool enable);
    /// Present the contents of the backbuffer.
    void Present();
    /// Return whether is initialized.
    bool IsInitialized() const { return context != nullptr; }
    /// Return current window size.
    IntVector2 Size() const;
    /// Return current window width.
    int Width() const { return Size().x; }
    /// Return current window height.
    int Height() const { return Size().y; }
    /// Return window render size, which can be different if the OS is doing resolution scaling.
    IntVector2 RenderSize() const;
    /// Return window render width.
    int RenderWidth() const { return RenderSize().x; }
    /// Return window render height.
    int RenderHeight() const { return RenderSize().y; }
    /// Return whether is fullscreen.
    bool IsFullscreen() const;
    /// Return whether is using vertical sync.
    bool VSync() const { return vsync; }
    /// Return the OS-level window.
    SDL_Window* Window() const { return window; }

private:
    /// OS-level rendering window.
    SDL_Window* window;
    /// OpenGL context.
    void* context;
    /// Vertical sync flag.
    bool vsync;
};

/// Register Graphics related object factories and attributes.
void RegisterGraphicsLibrary();

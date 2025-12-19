# Vimix Rendering Architecture Overview

## Executive Summary

Vimix is a real-time video mixing application with a sophisticated three-tier rendering pipeline:
1. **Source Rendering Layer** - Individual sources render to frame buffers via GStreamer
2. **Composition Layer** - Session compositing sources together into a single output frame
3. **UI/Display Layer** - OpenGL window rendering with ImGui UI overlay

The main rendering loop operates at a fixed ~62 FPS framerate with callbacks for each stage.

---

## 1. Main Application Loop

### Location
- **File**: `/home/bh/Claude/VimixDev/vimix/src/main.cpp`
- **Loop**: Lines 261-262

### Loop Structure
```cpp
// Main rendering loop (lines 261-262)
while ( Rendering::manager().isActive() )
    Rendering::manager().draw();
```

### Initialization Flow (lines 209-247)
```
1. Rendering::manager().init()          // OpenGL/GLFW initialization
2. UserInterface::manager().Init()       // ImGui setup
3. Audio::manager().initialize()         // Audio system
4. pushBackDrawCallback(prepare)         // Register update callback
5. pushBackDrawCallback(drawScene)       // Register scene rendering callback
6. pushBackDrawCallback(renderGUI)       // Register UI rendering callback
7. Rendering::manager().show()           // Display windows
8. Mixer::manager().load(_openfile)      // Load session
9. Main loop starts
```

---

## 2. Main Rendering Loop Implementation

### Location
- **File**: `/home/bh/Claude/VimixDev/vimix/src/RenderingManager.cpp`
- **Method**: `Rendering::draw()` (lines 608-673)

### Rendering Pipeline Flow

```
Rendering::draw()
    ├─ glfwPollEvents()                    // Poll input/window events
    │
    ├─ TabletInput::instance().pollEvents() // Tablet pressure support
    │
    ├─ main_.changeFullscreen_()           // Handle fullscreen requests
    │
    ├─ OUTPUT WINDOWS RENDERING
    │  └─ for (output windows):
    │      └─ output.draw(Mixer::session()->frame())  // Render to output windows
    │
    ├─ MAIN WINDOW CONTEXT
    │  └─ main_.makeCurrent()              // Switch to main window GL context
    │
    ├─ EXECUTE DRAW CALLBACKS (key stage)
    │  └─ for (draw_callbacks_):
    │      1. prepare()
    │         └─ Control::manager().update()
    │         └─ Mixer::manager().update()
    │         └─ UserInterface::manager().NewFrame()
    │
    │      2. drawScene()
    │         └─ Mixer::manager().draw()
    │
    │      3. renderGUI()
    │         └─ UserInterface::manager().Render()
    │
    ├─ SCREENSHOT CAPTURE
    │  └─ if (request_screenshot_):
    │      └─ screenshot_.captureGL()
    │
    └─ glfwSwapBuffers()                   // Display frame
    
    └─ FPS LIMITER (15600 microseconds)   // Soft-cap at ~62 FPS
       static GTimer for frame timing
```

### Framerate Control
- **Location**: Lines 666-672 in RenderingManager.cpp
- **Method**: GLib GTimer (g_timer_*)
- **Target**: ~62 FPS (15600 microseconds between frames)
- **Implementation**: Busy-sleep if frame completes too quickly

---

## 3. Scene Update and Rendering (Mixer)

### Location
- **File**: `/home/bh/Claude/VimixDev/vimix/src/Mixer.cpp`
- **Methods**: `Mixer::update()` (lines 86-276), `Mixer::draw()` (lines 278-283)

### Update Phase (`prepare()` callback)
Executed in `Mixer::update()`:
```
Mixer::update(dt_)
    ├─ Session::update(dt_)          // Update all sources
    │  └─ for (each source):
    │      └─ Source::update(dt)     // Update individual source
    │
    ├─ FrameGrabbing::grabFrame()    // Grab frames for recording
    │
    ├─ Process failed sources
    │  ├─ FAIL_RETRY: Attempt to recreate
    │  ├─ FAIL_CRITICAL: Detach from mixer
    │  └─ FAIL_FATAL: Delete source
    │
    └─ Update views for deep refresh
       └─ View::update(dt) for dirty views

Framerate tracking:
    dt_ = elapsed time since last frame (ms)
    dt__ = stabilized dt (EMA: 5% new + 95% old)
    fps = 1000 / dt__
```

### Draw Phase (`drawScene()` callback)
Executed in `Mixer::draw()`:
```
Mixer::draw()
    └─ current_view_->draw()         // Render current view
```

The actual rendering is delegated to the View's draw method.

---

## 4. Source Rendering Pipeline

### Source Architecture

**Base Class**: `/home/bh/Claude/VimixDev/vimix/src/Source/Source.h`

Source hierarchy:
```
Source (base class)
├─ MediaSource              // Video/image files via GStreamer
├─ DeviceSource             // Camera/webcam input
├─ StreamSource             // Generic GStreamer pipeline
├─ PatternSource            // Generated patterns
├─ TextSource               // Rendered text
├─ ShaderSource             // GLSL shader output
├─ RenderSource             // Renders nested session
├─ CloneSource              // References another source
├─ SessionSource            // Embedded session file
├─ ScreenCaptureSource      // Screen capture
├─ MultiFileSource          // Image sequence
├─ NetworkSource            // Network stream
└─ SrtReceiverSource        // SRT protocol input
```

### Source Update/Render Cycle

**Location**: Source.h (lines 162-169)

```cpp
// Each source has:
virtual void update(float dt);      // Called once per frame
virtual void render();              // Render to frame buffer
virtual FrameBuffer *frame() const; // Get output texture
```

### MediaSource (Primary Video Source) Flow

**File**: `/home/bh/Claude/VimixDev/vimix/src/Source/MediaSource.cpp`

```
MediaSource::update(dt)
    └─ mediaplayer_->update()       // Update GStreamer pipeline

MediaSource::init()
    └─ Create FrameBuffer matching media resolution
    └─ Link texture from MediaPlayer to source framebuffer

MediaSource::render()
    └─ Render media texture to source's frame buffer
```

---

## 5. GStreamer Integration

### MediaPlayer Class

**Location**: `/home/bh/Claude/VimixDev/vimix/src/MediaPlayer.h` (Lines 59-434)

Key components:
```cpp
struct MediaInfo {
    guint width, height, bitrate;
    guint framerate_n, framerate_d;
    std::string codec_name;
    bool isimage, interlaced, seekable;
    GstClockTime dt, end;    // Frame duration and end time
};

class MediaPlayer {
    GstElement *pipeline_;           // GStreamer pipeline
    GstBus *bus_;                    // Pipeline message bus
    GstAppSink *appsink_;            // Video output sink
    
    Frame frame_[N_VFRAME];          // Frame buffer stack
    uint textureid_;                 // OpenGL texture
    
    struct TimeCounter {
        GTimer *timer;               // FPS measurement
        gdouble fps;
    } timecount_;
};
```

### Pipeline Structure

**File**: `/home/bh/Claude/VimixDev/vimix/src/MediaPlayer.cpp`

```
GStreamer Pipeline:
    [uridecodebin]
        ├─ Video path:
        │   ├─ [videoscale]
        │   ├─ [videoconvert]
        │   ├─ [videofilter (optional)]
        │   └─ [glsinkbin] (GL rendering) or [appsink] (CPU)
        │
        └─ Audio path:
            └─ [audioconvert]
            └─ [appsink]
```

### GStreamer OpenGL Integration

**Location**: RenderingManager.cpp (lines 42-52, 172-221)

```cpp
// GL context sharing with GStreamer
#ifdef USE_GST_OPENGL_SYNC_HANDLER
static GstGLContext *global_gl_context = NULL;
static GstGLDisplay *global_display = NULL;

// Linking pipeline to rendering instance
void Rendering::LinkPipeline(GstPipeline *pipeline)
{
    GstBus* m_bus = gst_pipeline_get_bus(pipeline);
    gst_bus_set_sync_handler(m_bus, bus_sync_handler, pipeline, NULL);
}
```

### GPU Hardware Decoding

**Location**: RenderingManager.cpp (lines 500-515)

```cpp
// Enable GPU decoding if available
std::list<std::string> gpuplugins = 
    GstToolkit::enable_gpu_decoding_plugins(
        Settings::application.render.gpu_decoding);

// Supported plugins logged
if (Settings::application.render.gpu_decoding)
    Log::Info("Hardware decoding enabled.");
```

### Frame Handling

**File**: MediaPlayer.cpp

```cpp
// Frame stack with mutex protection
struct Frame {
    GstBuffer *buffer;
    FrameStatus status;    // SAMPLE, PREROLL, EOS, INVALID
    bool is_new;
    GstClockTime position;
    std::mutex access;
};
Frame frame_[N_VFRAME];    // N_VFRAME = 5 frames

// Callback when new sample arrives
static GstFlowReturn callback_new_sample(GstAppSink *sink, gpointer data)
{
    // Get sample, map to GPU texture
    // Fill texture via PBO (Pixel Buffer Object) upload
}
```

---

## 6. Session Composition and Output

### Session Class

**Location**: `/home/bh/Claude/VimixDev/vimix/src/Session.h`

```cpp
class Session {
    SourceList sources_;              // All sources in session
    RenderView render_;               // Composition renderer
    
    // Main output framebuffer
    FrameBuffer *frame() const { 
        return render_.frame(); 
    }
    
    void update(float dt);            // Update all sources
};
```

### RenderView (Composition)

**Location**: `/home/bh/Claude/VimixDev/vimix/src/View/RenderView.h`

```cpp
class RenderView : public View {
    FrameBuffer *frame_buffer_;       // Output framebuffer
    Surface *fading_overlay_;         // Fading effect overlay
    
    void draw() override;             // Composite all sources
};
```

### Composition Flow

**RenderView::draw()** (called from current View)

```
RenderView::draw()
    ├─ frame_buffer_->begin()         // Bind FBO
    │
    ├─ DrawVisitor traversal
    │  └─ Visit and draw all source nodes
    │     └─ Each source renders its texture to composition
    │
    ├─ Apply blending/effects
    │  └─ ImageProcessingShader
    │  └─ BlendingShader
    │
    ├─ Apply fading overlay
    │  └─ fading_overlay_->draw()
    │
    └─ frame_buffer_->end()           // Unbind FBO
    
Result: frame() returns the composed image
```

---

## 7. GUI Rendering (ImGui)

### Location
- **File**: `/home/bh/Claude/VimixDev/vimix/src/UserInterfaceManager.cpp`
- **Init**: `UserInterface::Init()` (lines 117-150)
- **Frame Start**: `UserInterface::NewFrame()` (lines 866-871)
- **Render**: `UserInterface::Render()` (lines 1046-1047)

### ImGui Integration

```cpp
// Initialization
ImGui::CreateContext();
ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init(VIMIX_GLSL_VERSION);

// Per-frame sequence (in renderGUI callback)
void UserInterface::NewFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UserInterface::Render()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
```

### UI Rendering Stages

```
renderGUI() callback
    ├─ UserInterface::NewFrame()
    │  └─ Poll input, start ImGui frame
    │
    ├─ Build UI (in UserInterface::Render())
    │  ├─ Main menu
    │  ├─ View navigator
    │  ├─ Source panels
    │  ├─ Output windows
    │  └─ Various controls
    │
    └─ ImGui::Render() + RenderDrawData()
       └─ Draw ImGui command buffer to OpenGL
```

---

## 8. Existing Timing and Performance Measurement

### GLib GTimer Usage

**Location**: Multiple files

```cpp
// RenderingManager.cpp (lines 667-671)
static GTimer *timer = g_timer_new();
double elapsed = g_timer_elapsed(timer, NULL) * 1000000.0;
if (elapsed < 15600.0 && elapsed > 0.0)
    g_usleep(15600 - (gulong)elapsed);
g_timer_start(timer);

// Mixer.cpp (lines 201-206)
static GTimer *timer = g_timer_new();
dt_ = g_timer_elapsed(timer, NULL) * 1000.0;  // Frame delta time in ms
dt__ = 0.05f * dt_ + 0.95f * dt__;           // Smoothed delta time (EMA)
g_timer_start(timer);
```

### FPS Calculation

**Location**: Mixer.h (line 87)

```cpp
inline int fps() const { return int(roundf(1000.f/dt__)); }
```

Also in MediaPlayer for individual sources:
```cpp
// MediaPlayer.h (lines 369-378)
struct TimeCounter {
    GTimer *timer;
    gdouble fps;
public:
    TimeCounter();
    ~TimeCounter();
    void tic();
    inline gdouble frameRate() const { return fps; }
};
```

### UI Metrics Display

**Location**: UserInterfaceManager.cpp

```cpp
#define PLOT_ARRAY_SIZE 180  // Rolling window of 180 frames

// RenderMetrics() method displays:
- Current FPS
- Frame time plot
- GPU memory usage
- Render statistics
```

### Memory Monitoring

**Location**: RenderingManager.cpp (lines 786-850)

```cpp
static glm::ivec2 getGPUMemoryInformation()
{
    // NVIDIA: GL_NVX_gpu_memory_info extension
    // ATI: GL_ATI_meminfo extension
    return { current_available_MB, total_available_MB }
}

static bool shouldHaveEnoughMemory(glm::vec3 resolution, int flags)
{
    // Check if enough VRAM before creating framebuffer
}
```

### Source Update Timing

**Location**: Mixer.cpp (lines 200-206)

```cpp
// Detailed frametime tracking
dt_ = g_timer_elapsed(timer, NULL) * 1000.0;   // Instantaneous
dt__ = 0.05f * dt_ + 0.95f * dt__;             // Exponential Moving Average

// Accessible via
Mixer::manager().dt()   // Raw delta
Mixer::manager().dt__() // Smoothed delta
Mixer::manager().fps()  // Calculated FPS
```

---

## 9. Callback Architecture

### Rendering Callbacks

**Location**: RenderingManager.h (lines 129-131)

```cpp
typedef void (* RenderingCallback)(void);
void pushBackDrawCallback(RenderingCallback function);

// Callbacks are executed in sequence each frame
std::list<RenderingCallback> draw_callbacks_;
```

### Current Registered Callbacks

**Location**: main.cpp (lines 241-243)

```cpp
1. prepare()           // Update phase
2. drawScene()         // Render scene phase
3. renderGUI()         // Render UI phase
```

Each callback is invoked in order during `Rendering::draw()` (lines 651-655).

---

## 10. Rendering Call Flow Diagram

```
┌─ Main Loop (main.cpp:261)
│
└─ Rendering::draw() [RenderingManager.cpp:608]
   │
   ├─ Event handling
   │  ├─ glfwPollEvents()
   │  ├─ TabletInput::pollEvents()
   │  └─ Handle fullscreen/window state changes
   │
   ├─ Output Windows Rendering
   │  └─ For each output window:
   │      └─ RenderingWindow::draw(frame)
   │          └─ Render session()->frame() to monitor
   │
   ├─ Main Window Context Activation
   │  └─ main_.makeCurrent()
   │
   ├─ Execute Draw Callbacks (in sequence)
   │  │
   │  ├─ [1] prepare() [main.cpp:46]
   │  │   └─ Mixer::update(dt) [Mixer.cpp:86]
   │  │      └─ Session::update(dt) [Session.h:96]
   │  │         ├─ For each source:
   │  │         │  ├─ Source::update(dt) [Source.h:162]
   │  │         │  │  ├─ MediaSource::update(dt) [MediaSource.cpp:181]
   │  │         │  │  │  └─ MediaPlayer::update() [MediaPlayer.cpp]
   │  │         │  │  │     └─ Poll GStreamer pipeline
   │  │         │  │  │     └─ Update texture from latest frame
   │  │         │  │  │     └─ Measure FPS via TimeCounter
   │  │         │  │  │
   │  │         │  │  └─ SourceCallback execution
   │  │         │  │
   │  │         │  ├─ RenderView internal update
   │  │         │  │  └─ Update mixing nodes positions/transforms
   │  │         │  │
   │  │         │  └─ View::update(dt) [View.h:38]
   │  │         │
   │  │         ├─ Frame delta time calculation
   │  │         │  └─ GTimer: dt_, dt__ (smoothed), fps
   │  │         │
   │  │         └─ Handle failed sources
   │  │
   │  │   └─ UserInterface::NewFrame() [UserInterfaceManager.cpp:866]
   │  │      ├─ ImGui_ImplOpenGL3_NewFrame()
   │  │      ├─ ImGui_ImplGlfw_NewFrame()
   │  │      └─ ImGui::NewFrame()
   │  │
   │  ├─ [2] drawScene() [main.cpp:53]
   │  │   └─ Mixer::draw() [Mixer.cpp:278]
   │  │      └─ current_view_->draw() [View.h:39]
   │  │         │
   │  │         └─ RenderView::draw() [RenderView.cpp]
   │  │            ├─ frame_buffer_->begin() [FrameBuffer.cpp:58]
   │  │            │  └─ Bind FBO, set viewport, clear
   │  │            │
   │  │            ├─ DrawVisitor traversal
   │  │            │  ├─ Visit scene root
   │  │            │  └─ Draw all source nodes
   │  │            │     └─ For each source:
   │  │            │         ├─ Source::render() [Source.h:169]
   │  │            │         │  └─ Render texture to composition FB
   │  │            │         │
   │  │            │         └─ Apply blending shader
   │  │            │            ├─ ImageProcessingShader
   │  │            │            └─ BlendingShader effects
   │  │            │
   │  │            ├─ Apply fading overlay
   │  │            │  └─ fading_overlay_->draw()
   │  │            │
   │  │            └─ frame_buffer_->end()
   │  │               └─ Unbind FBO
   │  │
   │  │            Result: Session frame buffer updated
   │  │
   │  └─ [3] renderGUI() [main.cpp:58]
   │      └─ UserInterface::Render() [UserInterfaceManager.cpp:1046]
   │         ├─ Build ImGui window hierarchy
   │         │  ├─ Menu bar
   │         │  ├─ View navigator
   │         │  ├─ Source control panels
   │         │  ├─ Inspector windows
   │         │  └─ Other UI elements
   │         │
   │         └─ ImGui::Render() + RenderDrawData()
   │            └─ Execute OpenGL draw calls for UI
   │
   ├─ Screenshot Capture (if requested)
   │  └─ screenshot_.captureGL()
   │
   ├─ Buffer Swap
   │  └─ glfwSwapBuffers()
   │
   └─ Framerate Limiting
      └─ GTimer: sleep until 15600 microseconds elapsed (~62 FPS)
```

---

## 11. Key Files and Their Roles

| Component | File | Key Responsibility |
|-----------|------|-------------------|
| **Main Loop** | `main.cpp` | Application entry, callback registration |
| **Rendering Manager** | `RenderingManager.cpp/.h` | Window management, GL context, callback execution |
| **Mixer** | `Mixer.cpp/.h` | Scene management, source updating, FPS calculation |
| **Session** | `Session.cpp/.h` | Source composition, session state |
| **RenderView** | `View/RenderView.cpp/.h` | Final composition/rendering to FBO |
| **Source Base** | `Source/Source.cpp/.h` | Source interface, framebuffer attachment |
| **MediaSource** | `Source/MediaSource.cpp` | Video/image file handling |
| **MediaPlayer** | `MediaPlayer.cpp/.h` | GStreamer pipeline control, texture management |
| **Stream** | `Stream.cpp/.h` | Generic GStreamer stream handling |
| **FrameBuffer** | `FrameBuffer.cpp/.h` | OpenGL FBO creation and management |
| **UI Manager** | `UserInterfaceManager.cpp/.h` | ImGui initialization and rendering |
| **DrawVisitor** | `Visitor/DrawVisitor.cpp/.h` | Scene graph traversal for rendering |

---

## 12. Profiling Opportunities

### Existing Instrumentation Points

1. **GLib GTimer** - Used for frame timing
   - Location: `RenderingManager.cpp`, `Mixer.cpp`
   - Already measures: Frame delta time, FPS, framerate limiting

2. **MediaPlayer TimeCounter** - FPS per source
   - Location: `MediaPlayer.h` (lines 370-378)
   - Already measures: Update framerate per media player

3. **GPU Memory** - VRAM usage queries
   - Location: `RenderingManager.cpp` (getGPUMemoryInformation)
   - Uses: NVIDIA/ATI GPU memory extensions

### Recommended Profiling Points

1. **Per-stage timing** (prepare, draw, GUI)
   - Insert GTimer at start/end of each callback
   - Track: Update time, render time, UI time

2. **Source-level profiling**
   - Time each `Source::update()` and `Source::render()`
   - Track: GStreamer decoding, texture upload, GPU rendering

3. **View composition timing**
   - Time RenderView::draw() phases
   - Track: DrawVisitor traversal, shader execution

4. **GStreamer pipeline monitoring**
   - Query pipeline state and buffer queue depth
   - Track: Frame drops, decoding lag

5. **ImGui rendering stats**
   - Use ImGui metrics window
   - Track: Draw call count, vertex count, UI layout time

---

## 13. Architecture Summary

**Vimix's rendering is organized as:**

1. **Event-Driven Main Loop**
   - Framerate-limited (~62 FPS)
   - Callback-based architecture

2. **Three-Stage Pipeline Per Frame**
   - Update phase: Process all sources and state
   - Render phase: Compose sources to session framebuffer
   - UI phase: Render ImGui overlay

3. **GStreamer-Based Source Handling**
   - Asynchronous decoding via GStreamer pipelines
   - GPU texture management (GL interop)
   - Hardware decoding support

4. **Layered Rendering Model**
   - Individual source → source framebuffer
   - All sources → session framebuffer (composition)
   - Session → output windows (display)
   - UI overlay on top (ImGui)

5. **Visitor Pattern for Scene Traversal**
   - DrawVisitor renders scene graph
   - Depth-sorted source ordering
   - Flexible for different rendering modes

This architecture enables efficient real-time video mixing with flexible source types and advanced effects processing.


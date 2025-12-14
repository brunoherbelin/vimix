# Vimix Rendering Architecture Analysis - Index

This directory contains comprehensive documentation of the vimix rendering architecture and a proposal for adding profiling capabilities.

## Documents

### 1. RENDERING_ARCHITECTURE.md
**Comprehensive technical documentation of the entire rendering system**

- **13 Sections, 717 lines**
- Executive summary of the three-tier rendering pipeline
- Main application loop structure and initialization
- Main rendering loop implementation with detailed flow diagrams
- Scene update and rendering (Mixer class)
- Complete source rendering pipeline (12 source types)
- GStreamer integration architecture and pipeline structure
- Session composition and RenderView
- GUI rendering with ImGui integration
- Existing timing and performance measurement
- Callback architecture design
- Complete rendering call flow diagram (section 10)
- Key files and their roles (reference table)
- Profiling opportunities assessment
- Architecture summary

**Key Highlights:**
- All file paths and line numbers are referenced
- ASCII flow diagrams showing complete rendering sequence
- GStreamer pipeline structure documented
- Frame timing methodology explained (~62 FPS target)
- All 12 source types catalogued

### 2. PROFILING_PROPOSAL.md
**Actionable proposal for implementing a performance profiling system**

- **357 lines with implementation examples**
- Current state assessment (existing timing infrastructure)
- Proposed PerformanceProfiler singleton design
- Architecture diagrams showing proposed components
- Detailed implementation specifications with code examples
- Stage-level profiling design
- Source-level profiling design
- GStreamer monitoring integration
- GPU metrics collection
- Historical data storage strategy
- UI integration with ImGui examples
- Benefits analysis
- 4-phase implementation roadmap
- Minimal MVP specification (~200 lines)
- Backward compatibility notes
- References to APIs and libraries

**Implementation Phases:**
- Phase 1: Core infrastructure (2 weeks)
- Phase 2: Source profiling (1 week)
- Phase 3: GStreamer integration (1 week)
- Phase 4: Advanced features (ongoing)

## Quick Reference

### Main Entry Points

| Component | File | Function |
|-----------|------|----------|
| Main Loop | `src/main.cpp` | `main()` (line 63) |
| Render Loop | `src/main.cpp` | Loop (lines 261-262) |
| Frame Rendering | `src/RenderingManager.cpp` | `Rendering::draw()` (line 608) |
| Scene Update | `src/Mixer.cpp` | `Mixer::update()` (line 86) |
| Scene Rendering | `src/Mixer.cpp` | `Mixer::draw()` (line 278) |
| UI Rendering | `src/UserInterfaceManager.cpp` | `UserInterface::Render()` (line 1046) |
| Composition | `src/View/RenderView.cpp` | `RenderView::draw()` |
| GStreamer | `src/MediaPlayer.cpp` | `MediaPlayer::update()` |

### Performance Measurements

**Already Implemented:**
- Main framerate: GLib GTimer (~62 FPS)
- Frame delta time: `Mixer::dt_`, `Mixer::dt__`
- Per-source FPS: `MediaPlayer::TimeCounter`
- GPU memory: `Rendering::getGPUMemoryInformation()`

**Proposed:**
- Per-stage timing breakdown
- Per-source update/render timing
- GStreamer pipeline metrics
- GPU draw call counts
- Historical trend analysis

### Architecture Overview

```
Application Main Loop
    ↓
Rendering::draw() [RenderingManager.cpp:608]
    ├─ Event handling (glfwPollEvents, tablet input)
    ├─ Output window rendering
    ├─ Execute draw callbacks:
    │   ├─ prepare() → Mixer::update() → Session::update()
    │   ├─ drawScene() → Mixer::draw() → RenderView::draw()
    │   └─ renderGUI() → UserInterface::Render()
    ├─ Buffer swap (glfwSwapBuffers)
    └─ Framerate limiting (~62 FPS via GTimer)
```

### Source Types (12 Total)

1. MediaSource - Video/image files
2. DeviceSource - Cameras/webcams
3. StreamSource - Generic GStreamer pipelines
4. PatternSource - Generated patterns
5. TextSource - Rendered text
6. ShaderSource - GLSL shader output
7. RenderSource - Nested session rendering
8. CloneSource - Source references
9. SessionSource - Embedded session files
10. ScreenCaptureSource - Screen capture
11. MultiFileSource - Image sequences
12. NetworkSource - Network streams
13. SrtReceiverSource - SRT protocol input

### GStreamer Pipeline Structure

```
[uridecodebin]
├─ Video: [videoscale] → [videoconvert] → [glsinkbin/appsink]
└─ Audio: [audioconvert] → [appsink]
```

Features:
- Hardware decoding (NVIDIA, AMD, Intel)
- GL context sharing
- Frame stack (N_VFRAME=5)
- Per-frame FPS tracking
- Async decoding

## How to Use This Documentation

### For Understanding the Architecture:
1. Start with RENDERING_ARCHITECTURE.md sections 1-3
2. Review the call flow diagram (section 10)
3. Study the key files table (section 11)
4. Deep dive into specific sections for details

### For Implementing Profiling:
1. Read PROFILING_PROPOSAL.md overview
2. Review proposed architecture
3. Follow implementation phases
4. Use code examples as templates
5. Minimal MVP can be implemented in ~200 lines

### For Performance Optimization:
1. Understand current measurements (section 8 of RENDERING_ARCHITECTURE.md)
2. Implement profiling system (PROFILING_PROPOSAL.md)
3. Use profiling data to identify bottlenecks
4. Focus optimization efforts on hot paths

## Key Insights

### Rendering Pipeline Characteristics:
- **Event-driven** callback architecture
- **Three-stage** per-frame design (update/render/UI)
- **Real-time** ~62 FPS soft cap
- **Layered** rendering model
- **Modular** source system

### Performance Considerations:
- GStreamer handles async decoding
- GPU texture management with FBOs
- Hardware decoding support
- CPU framerate limiting
- No V-Sync (soft-limit only)

### Design Patterns Used:
- Visitor pattern (scene graph rendering)
- Singleton pattern (managers)
- Callback pattern (rendering stages)
- Factory pattern (source creation)

## References

### Documentation
- GLib GTimer: https://developer.gnome.org/glib/stable/glib-Timers.html
- GStreamer: https://gstreamer.freedesktop.org/
- ImGui: https://github.com/ocornut/imgui
- OpenGL: https://www.khronos.org/opengl/

### Code Files Analyzed
- Total files examined: 50+
- Main source files: 15+
- Header files: 10+
- Visitor implementations: 8
- Window/UI components: 5+

## Next Steps

For developers looking to work with vimix:

1. **Understanding Performance**
   - Implement profiling system (see PROFILING_PROPOSAL.md)
   - Use profiling metrics for optimization
   - Track performance regressions

2. **Adding New Source Type**
   - Review Source base class (Source/Source.h)
   - Implement update() and render() methods
   - Attach FrameBuffer for output
   - Register in Mixer::createSource*()

3. **Optimizing Rendering**
   - Use profiling to identify hot paths
   - Consider GPU batching
   - Optimize shader compilation
   - Profile GStreamer pipelines

## Document Statistics

| Document | Lines | Sections | Code Examples | Diagrams |
|----------|-------|----------|----------------|----------|
| RENDERING_ARCHITECTURE.md | 717 | 13 | 20+ | 3 |
| PROFILING_PROPOSAL.md | 357 | 8 | 10+ | 2 |
| **Total** | **1,074** | **21** | **30+** | **5** |

---

*Analysis completed: December 12, 2024*
*Analyzed with Claude Code*

# Vimix Profiling System Proposal

## Overview

Based on the comprehensive rendering architecture analysis, this document proposes a profiling system to measure and monitor vimix rendering performance.

## Current State

### Existing Timing Infrastructure

Vimix already has basic timing measurements:

1. **Main Framerate**: GLib GTimer in `RenderingManager::draw()` (~62 FPS target)
2. **Frame Delta Time**: `Mixer::dt_` (raw) and `dt__` (smoothed EMA)
3. **Per-Source FPS**: `MediaPlayer::TimeCounter` for individual media sources
4. **GPU Memory**: NVIDIA/ATI extension queries in `Rendering::getGPUMemoryInformation()`
5. **UI Metrics Window**: ImGui built-in metrics display (180-frame rolling window)

### Limitations

- No per-stage breakdown (update vs render vs UI phases)
- Limited source-level visibility
- No GStreamer pipeline monitoring
- No texture upload/GPU overhead tracking
- No frame drop detection
- Limited historical data retention

## Proposed Profiling System

### Architecture

Create a new `PerformanceProfiler` singleton that instruments the three main rendering stages:

```
PerformanceProfiler (new)
├─ Stage Timers
│  ├─ Update phase (Mixer::update)
│  ├─ Render phase (RenderView::draw)
│  └─ UI phase (UserInterface::Render)
├─ Source Profilers
│  ├─ Per-source update time
│  └─ Per-source render time
├─ GStreamer Metrics
│  ├─ Pipeline state monitoring
│  ├─ Buffer queue depth
│  └─ Frame drop detection
├─ GPU Metrics
│  ├─ VRAM usage
│  ├─ Draw call counts
│  └─ Shader compilation times
└─ Historical Data
   ├─ Rolling window storage (configurable)
   ├─ Statistical aggregation
   └─ Export capabilities
```

### Key Files to Modify

1. **New File**: `src/PerformanceProfiler.h` / `src/PerformanceProfiler.cpp`
2. **RenderingManager.cpp**: Add timer checkpoints in `draw()` method
3. **Mixer.cpp**: Add source-level timing in update/render
4. **MediaPlayer.cpp**: Enhance GStreamer monitoring
5. **UserInterfaceManager.cpp**: Add UI stage profiling

### Detailed Proposal

#### 1. Stage-Level Profiling

```cpp
// In RenderingManager::draw()
timer.markStageStart("update");
// ... execute prepare callback ...
timer.markStageEnd("update");

timer.markStageStart("render");
// ... execute drawScene callback ...
timer.markStageEnd("render");

timer.markStageStart("ui");
// ... execute renderGUI callback ...
timer.markStageEnd("ui");
```

#### 2. Source-Level Profiling

```cpp
// In Source::update() and Source::render()
PerformanceProfiler::manager().markSourceStart(id(), "update");
// ... source-specific update ...
PerformanceProfiler::manager().markSourceEnd(id(), "update");
```

#### 3. GStreamer Monitoring

```cpp
// In MediaPlayer::update()
struct PipelineMetrics {
    GstState current_state;
    guint buffer_queue_depth;
    guint dropped_frames;
    guint total_frames;
    gdouble decoder_time_ms;
};

// Query pipeline statistics
auto metrics = gst_pipeline_get_metrics(pipeline_);
profiler.recordPipelineMetrics(id_, metrics);
```

#### 4. GPU Metrics Collection

```cpp
struct GPUMetrics {
    glm::ivec2 memory;           // { available, total }
    GLint draw_call_count;
    GLint vertex_count;
    GLint texture_count;
};

// Collected via GL queries
profiler.recordGPUMetrics(metrics);
```

#### 5. Historical Data Storage

```cpp
// Circular buffer for 10 seconds of history at 62 FPS
#define HISTORY_DURATION_FRAMES (62 * 10)  // 620 frames

struct FrameProfile {
    uint64_t timestamp;
    
    struct {
        double total;
        double update;
        double render;
        double ui;
        double overhead;
    } timing;
    
    std::map<uint64_t, SourceProfile> sources;
    GPUMetrics gpu;
    std::map<uint64_t, PipelineMetrics> pipelines;
};

FrameProfile history[HISTORY_DURATION_FRAMES];
```

### Implementation Details

#### Create `PerformanceProfiler` Class

```cpp
// PerformanceProfiler.h
class PerformanceProfiler {
public:
    static PerformanceProfiler& manager() {
        static PerformanceProfiler _instance;
        return _instance;
    }
    
    // Stage marking
    void markStageStart(const std::string& stage);
    void markStageEnd(const std::string& stage);
    
    // Source profiling
    void markSourceStart(uint64_t source_id, const std::string& operation);
    void markSourceEnd(uint64_t source_id, const std::string& operation);
    
    // Metrics recording
    void recordGPUMetrics(const GPUMetrics& metrics);
    void recordPipelineMetrics(uint64_t id, const PipelineMetrics& metrics);
    void recordFrameProfile();  // Called at end of frame
    
    // Data access
    FrameProfile getFrameProfile(int frame_offset = 0) const;
    std::vector<FrameProfile> getHistory(int num_frames = 60) const;
    FrameProfile getAggregateStats(int num_frames = 60) const;
    
    // Export
    void exportToJSON(const std::string& filename) const;
    void exportToCSV(const std::string& filename) const;

private:
    std::map<std::string, GTimer*> stage_timers_;
    std::map<std::string, guint64> stage_times_;
    
    FrameProfile current_frame_;
    FrameProfile history[HISTORY_DURATION_FRAMES];
    int history_index_;
    
    std::mutex access_;
};
```

#### Integration Points

**RenderingManager.cpp - Main Loop**
```cpp
void Rendering::draw() {
    PerformanceProfiler::manager().recordFrameProfile();  // Start new frame
    
    // ... event handling ...
    
    PerformanceProfiler::manager().markStageStart("update");
    
    for (iter=draw_callbacks_.begin(); iter != draw_callbacks_.end(); ++iter) {
        (*iter)();
    }
    
    PerformanceProfiler::manager().markStageEnd("update");
    
    // ... output windows rendering ...
    
    // ... screenshot ...
    
    glfwSwapBuffers(main_.window());
    
    // ... framerate limiting ...
}
```

**Mixer.cpp - Source Update**
```cpp
void Source::update(float dt) {
    PerformanceProfiler::manager().markSourceStart(id(), "update");
    
    // ... existing update code ...
    
    PerformanceProfiler::manager().markSourceEnd(id(), "update");
}
```

**MediaPlayer.cpp - GStreamer Monitoring**
```cpp
void MediaPlayer::update() {
    // ... existing update code ...
    
    PipelineMetrics metrics;
    gst_element_get_state(pipeline_, &metrics.current_state, nullptr, 0);
    // Query other metrics...
    
    PerformanceProfiler::manager().recordPipelineMetrics(id_, metrics);
}
```

### UI Integration

Add a new ImGui window to display profiling data:

```cpp
// In UserInterfaceManager.cpp
void RenderProfilerWindow(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Performance Profiler", p_open)) {
        auto& profiler = PerformanceProfiler::manager();
        
        // Current frame timing
        auto frame = profiler.getFrameProfile();
        ImGui::Text("Frame Time: %.2f ms (%.1f FPS)", 
                   frame.timing.total, 1000.0 / frame.timing.total);
        
        // Stage breakdown
        ImGui::Separator();
        ImGui::Text("Update: %.2f ms", frame.timing.update);
        ImGui::Text("Render: %.2f ms", frame.timing.render);
        ImGui::Text("UI:     %.2f ms", frame.timing.ui);
        
        // Source list
        ImGui::Separator();
        ImGui::Text("Sources:");
        for (auto& [id, src_prof] : frame.sources) {
            ImGui::Text("  [%llu] Update: %.2f ms, Render: %.2f ms",
                       id, src_prof.update_time, src_prof.render_time);
        }
        
        // GPU metrics
        ImGui::Separator();
        ImGui::Text("GPU Memory: %d / %d MB", 
                   frame.gpu.memory.x, frame.gpu.memory.y);
        
        // Historical graph
        auto history = profiler.getHistory(60);
        std::vector<float> times;
        for (auto& f : history) {
            times.push_back(f.timing.total);
        }
        ImGui::PlotLines("Frame Time", times.data(), times.size());
    }
    ImGui::End();
}
```

## Benefits

1. **Performance Insight**
   - Identify bottlenecks (update vs render vs UI)
   - Per-source profiling for slow sources
   - GStreamer pipeline health monitoring

2. **Debugging**
   - Frame drop detection and root cause analysis
   - GPU memory pressure tracking
   - Source failure diagnosis

3. **Optimization**
   - Data-driven optimization decisions
   - Historical trend analysis
   - Comparative performance testing

4. **Reporting**
   - Export capabilities for analysis
   - Automated performance reports
   - Baseline comparison

## Implementation Phases

### Phase 1: Core Infrastructure (Week 1-2)
- Create PerformanceProfiler class
- Add stage-level timing (update/render/UI)
- Basic UI window

### Phase 2: Source Profiling (Week 2-3)
- Per-source update/render timing
- Source-level UI metrics

### Phase 3: GStreamer Integration (Week 3-4)
- Pipeline state monitoring
- Buffer queue tracking
- Frame drop detection

### Phase 4: Advanced Features (Week 4+)
- GPU metrics integration
- Export functionality
- Historical analysis
- Optimization recommendations

## Minimal Implementation

For a quick MVP:
1. Add GTimer checkpoints in 3 main stages
2. Simple ImGui window showing percentages
3. Rolling 60-frame history display
4. ~200 lines of code total

## Backward Compatibility

- All changes are additive (no API breaking changes)
- Profiling is optional (can be compiled out)
- Negligible performance impact when disabled

## References

- GLib GTimer API: https://developer.gnome.org/glib/stable/glib-Timers.html
- ImGui Plotting: https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp
- GStreamer Debug API: https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html

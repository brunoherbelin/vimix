# Transcoder Usage

The `Transcoder` class provides simple video transcoding functionality using GStreamer.

## Features

- Transcodes any video format to H.264/MP4
- Automatically generates output filename (non-destructive)
- Provides progress monitoring
- Asynchronous operation

## Basic Usage

```cpp
#include "Transcoder.h"

// Create transcoder with input file
Transcoder transcoder("/path/to/input/video.mov");

// Output filename is automatically generated
std::cout << "Output will be: " << transcoder.outputFilename() << std::endl;
// Output: /path/to/input/video_transcoded.mp4

// Start transcoding with default options
if (!transcoder.start()) {
    std::cerr << "Failed to start: " << transcoder.error() << std::endl;
    return 1;
}

// Monitor progress
while (!transcoder.finished()) {
    double progress = transcoder.progress();  // 0.0 to 1.0
    std::cout << "Progress: " << (progress * 100) << "%" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

// Check result
if (transcoder.success()) {
    std::cout << "Success! Output: " << transcoder.outputFilename() << std::endl;
} else {
    std::cerr << "Failed: " << transcoder.error() << std::endl;
}
```

## Advanced Usage with Options

```cpp
#include "Transcoder.h"

// Create transcoder
Transcoder transcoder("/path/to/animation.mov");

// Configure transcoding options
TranscoderOptions options;
options.force_keyframes = true;              // Keyframe every second for easier editing
options.psy_tune = PsyTuning::ANIMATION;     // Optimize for animation content

// Start with options
if (!transcoder.start(options)) {
    std::cerr << "Failed: " << transcoder.error() << std::endl;
    return 1;
}

// Wait for completion...
while (!transcoder.finished()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
```

## Common Configuration Examples

### For Video Editing (Maximum Keyframes)
```cpp
TranscoderOptions options;
options.force_keyframes = true;  // Keyframe every ~1 second
// Makes timeline scrubbing and frame-accurate editing much easier
transcoder.start(options);
```

### For Film Content
```cpp
TranscoderOptions options;
options.psy_tune = PsyTuning::FILM;  // Optimized for film/camera footage
transcoder.start(options);
```

### For Animation
```cpp
TranscoderOptions options;
options.psy_tune = PsyTuning::ANIMATION;  // Optimized for flat colors and sharp edges
transcoder.start(options);
```

### For Grain Preservation
```cpp
TranscoderOptions options;
options.psy_tune = PsyTuning::GRAIN;  // Preserve film grain detail
transcoder.start(options);
```

## API Reference

### Constructor
```cpp
Transcoder(const std::string& input_filename)
```
Creates a transcoder for the specified input file. Output filename is automatically generated.

### Enums and Structs

#### `PsyTuning` enum
Psycho-visual tuning options for x264 encoder:
- `NONE` - No specific tuning (default)
- `FILM` - Optimize for film/live action content
- `ANIMATION` - Optimize for animation with flat colors and sharp edges
- `GRAIN` - Preserve film grain detail
- `STILL_IMAGE` - Optimize for slideshow-style content

#### `TranscoderOptions` struct
Configuration options for transcoding:
- `bool force_keyframes` - Force keyframe every ~1 second (default: false)
  - When enabled: sets `key-int-max=30` (keyframe every second at 30fps)
  - When disabled: sets `key-int-max=250` (default, less frequent)
  - Useful for video editing to enable frame-accurate seeking
- `PsyTuning psy_tune` - Psycho-visual tuning preset (default: NONE)

### Methods

#### `bool start(const TranscoderOptions& options = TranscoderOptions())`
Start the transcoding process with optional configuration.
- Parameters:
  - `options` - Transcoding configuration (keyframes, tuning, etc.)
- Returns: `true` if started successfully, `false` on error

#### `bool finished() const`
Check if transcoding has completed.
- Returns: `true` when transcoding is done (success or error)

#### `bool success() const`
Check if transcoding completed successfully.
- Returns: `true` only if finished AND successful

#### `double progress() const`
Get current transcoding progress.
- Returns: Value between 0.0 (starting) and 1.0 (complete)

#### `const std::string& inputFilename() const`
Get the input filename.

#### `const std::string& outputFilename() const`
Get the generated output filename.

#### `const std::string& error() const`
Get error message if transcoding failed.
- Returns: Error message string, empty if no error

## Output Filename Generation

The transcoder automatically generates output filenames:
- Input: `/path/to/video.mov`
- Output: `/path/to/video_transcoded.mp4`

If the file exists, it adds a number:
- `/path/to/video_transcoded_1.mp4`
- `/path/to/video_transcoded_2.mp4`
- etc.

## Technical Details

### Pipeline
The transcoder uses the following GStreamer pipeline:
```
filesrc → decodebin → videoconvert → x264enc → mp4mux → filesink
                   ├→ audioconvert → avenc_aac ┘
```

### Encoding Settings
- Video codec: H.264 (x264enc)
- Audio codec: AAC (avenc_aac)
- Container: MP4
- Speed preset: medium
- Keyframes: Configurable via `force_keyframes` option
  - Default: `key-int-max=250` (keyframe every 250 frames)
  - Force mode: `key-int-max=30` (keyframe every 30 frames, ~1 second at 30fps)
- Psycho-visual tuning: Configurable via `psy_tune` option

### Requirements
- GStreamer 1.0+
- gstreamer-base
- gstreamer-pbutils
- x264 encoder plugin
- AAC encoder plugin

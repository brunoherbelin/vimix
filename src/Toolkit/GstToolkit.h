#ifndef __GSTGUI_TOOLKIT_H_
#define __GSTGUI_TOOLKIT_H_

#include <gst/gst.h>

#include <string>
#include <list>
#include <set>

namespace GstToolkit
{

typedef enum {
    TIME_STRING_FIXED = 0,
    TIME_STRING_ADJUSTED,
    TIME_STRING_MINIMAL,
    TIME_STRING_READABLE
} time_string_mode;

std::string time_to_string(guint64 t, time_string_mode m = TIME_STRING_ADJUSTED);
std::string filename_to_uri(std::string filename);

std::string gst_version();

std::list<std::string> all_plugins();
std::list<std::string> enable_gpu_decoding_plugins(bool enable = true);
std::string used_gpu_decoding_plugins(GstElement *gstbin);
std::string used_decoding_plugins(GstElement *gstbin);

std::list<std::string> all_plugin_features(const std::string &pluginname);
bool has_feature (const std::string &name);
bool enable_feature (const std::string &name, bool enable);


struct PipelineConfig {
    gint width;
    gint height;
    gint fps_numerator;
    gint fps_denominator;
    std::string stream;
    std::string format;

    PipelineConfig() {
        width = 0;
        height = 0;
        fps_numerator = 30;
        fps_denominator = 1;
        stream = "";
        format = "";
    }

    inline bool operator < (const PipelineConfig b) const
    {
        int formatscore = this->format.find("R") != std::string::npos ? 2 : 1; // best score for RGBx
        int b_formatscore = b.format.find("R") != std::string::npos ? 2 : 1;
        float fps = static_cast<float>(this->fps_numerator) / static_cast<float>(this->fps_denominator);
        float b_fps = static_cast<float>(b.fps_numerator) / static_cast<float>(b.fps_denominator);
        return ( fps * static_cast<float>(this->width * this->height * formatscore) 
                    < b_fps * static_cast<float>(b.width * b.height * b_formatscore));
    }
};

struct better_config_comparator
{
    inline bool operator () (const PipelineConfig a, const PipelineConfig b) const
    {
        return (a < b);
    }
};

typedef std::set<PipelineConfig, better_config_comparator> PipelineConfigSet;

PipelineConfigSet getPipelineConfigs(const std::string &src_description);

// Download url to dest with GStreamer (souphttpsrc follows the HTTPS
// redirects), via a .part file renamed only on success so an interrupted
// run never leaves a truncated file behind. Throws std::runtime_error on
// failure.
void download_file(const std::string &url, const std::string &dest);

// Functional probe of a GStreamer encoder: 
// Encodes one tiny test frame end-to-end and checks for EOS.
// Returns true if the encoder works, false otherwise.
bool encoder_works(const char *enc);

// Raises a GStreamer debug category's threshold for its lifetime, restoring it
// on destruction. Used to mute one chatty, benign category around a call
// without touching global logging or any other category.
class GstCategoryHush {
public:
    GstCategoryHush(const char *category, GstDebugLevel level) : name_(category)
    {
        gst_debug_set_threshold_for_name(category, level);
    }
    ~GstCategoryHush() { gst_debug_unset_threshold_for_name(name_); }
    GstCategoryHush(const GstCategoryHush &) = delete;
    GstCategoryHush &operator=(const GstCategoryHush &) = delete;
private:
    const char *name_;
};

}

#endif // __GSTGUI_TOOLKIT_H_

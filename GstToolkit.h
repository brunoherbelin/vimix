#ifndef __GSTGUI_TOOLKIT_H_
#define __GSTGUI_TOOLKIT_H_

#include <gst/gst.h>

#include <string>
#include <list>

namespace GstToolkit
{

    std::string time_to_string(guint64 t);

    std::string gst_version();
    std::list<std::string> all_plugins();
    std::list<std::string> all_plugin_features(std::string pluginname);

    bool enable_feature (std::string name, bool enable);

}

#endif // __GSTGUI_TOOLKIT_H_

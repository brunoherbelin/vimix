#ifndef __GSTGUI_TOOLKIT_H_
#define __GSTGUI_TOOLKIT_H_

#include <string>
#include <list>
using namespace std;


namespace GstToolkit
{

    string to_string(guint64 t);

    list<string> all_plugins();
    list<string> all_plugin_features(string pluginname);

    bool enable_feature (string name, bool enable);

    string gst_version();

}

#endif // __GSTGUI_TOOLKIT_H_

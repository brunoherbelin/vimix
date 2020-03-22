
#include <sstream>
#include <iomanip>

#include <gst/gst.h>

#include "GstToolkit.h"


string GstToolkit::to_string(guint64 t) {

    if (t == GST_CLOCK_TIME_NONE)
        return "00:00:00.00";

    guint ms =  GST_TIME_AS_MSECONDS(t);
    guint s = ms / 1000;

    std::ostringstream oss;
    if (s / 3600)
        oss << std::setw(2) << std::setfill('0') << s / 3600 << ':';
    if ((s % 3600) / 60)
        oss << std::setw(2) << std::setfill('0') << (s % 3600) / 60 << ':';
    oss << std::setw(2) << std::setfill('0') << (s % 3600) % 60 << '.';
    oss << std::setw(2) << std::setfill('0') << (ms % 1000) / 10;

    return oss.str();
}


list<string> GstToolkit::all_plugins()
{
    list<string> pluginlist;
    GList *l, *g;

    l = gst_registry_get_plugin_list (gst_registry_get ());

    for (g = l; g; g = g->next) {
        GstPlugin *plugin = GST_PLUGIN (g->data);
        pluginlist.push_front( string( gst_plugin_get_name (plugin) ) );
    }

    gst_plugin_list_free (l);

    return pluginlist;
}


list<string> GstToolkit::all_plugin_features(string pluginname)
{
    list<string> featurelist;
    GList *l, *g;

    l = gst_registry_get_feature_list_by_plugin (gst_registry_get (), pluginname.c_str());

    for (g = l; g; g = g->next) {
        GstPluginFeature *feature = GST_PLUGIN_FEATURE (g->data);
        featurelist.push_front( string( gst_plugin_feature_get_name (feature) ) );
    }

    gst_plugin_feature_list_free (l);

    return featurelist;
}

bool GstToolkit::enable_feature (string name, bool enable) {
    GstRegistry *registry = NULL;
    GstElementFactory *factory = NULL;

    registry = gst_registry_get();
    if (!registry) return false;

    factory = gst_element_factory_find (name.c_str());
    if (!factory) return false;

    if (enable) {
        gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), GST_RANK_PRIMARY + 1);
    }
    else {
        gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE (factory), GST_RANK_NONE);
    }

    gst_registry_add_feature (registry, GST_PLUGIN_FEATURE (factory));
    return true;
}


string GstToolkit::gst_version()
{
    std::ostringstream oss;
    oss << GST_VERSION_MAJOR << '.' << GST_VERSION_MINOR << '.';
    oss << std::setw(2) << std::setfill('0') << GST_VERSION_MICRO ;
    if (GST_VERSION_NANO > 0)
        oss << ( (GST_VERSION_NANO < 2 ) ? " - (CVS)" : " - (Prerelease)");

    return oss.str();
}

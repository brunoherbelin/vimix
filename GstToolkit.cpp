
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

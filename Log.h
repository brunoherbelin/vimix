#ifndef __LOG_H_
#define __LOG_H_

namespace Log
{
    // log info
    void Info(const char* fmt, ...);
    // Draw logs
    void ShowLogWindow(bool* p_open = nullptr);
};

#endif // __LOG_H_
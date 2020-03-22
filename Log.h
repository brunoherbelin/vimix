#ifndef __LOG_H_
#define __LOG_H_

namespace Log
{
    // log
    void Info(const char* fmt, ...);
    void Warning(const char* fmt, ...);
    void Error(const char* fmt, ...);

    // Draw logs
    void ShowLogWindow(bool* p_open = nullptr);

    void Render();
}

#endif // __LOG_H_

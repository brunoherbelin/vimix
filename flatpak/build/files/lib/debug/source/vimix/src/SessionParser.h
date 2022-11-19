#ifndef SESSIONPARSER_H
#define SESSIONPARSER_H

#include <map>
#include <string>
#include <tinyxml2.h>


class Session;

//struct SessionInformation {
//    std::string description;
//    FrameBufferImage *thumbnail;
//    bool user_thumbnail_;
//    SessionInformation() {
//        description = "";
//        thumbnail = nullptr;
//        user_thumbnail_ = false;
//    }
//};

class SessionParser
{
public:
    SessionParser();

    bool open(const std::string& filename);
    bool save();

    std::map<uint64_t, std::pair<std::string, bool> > pathList() const;
    void replacePath(uint64_t id, const std::string &path);

//    static SessionInformation info(const std::string& filename);

private:
    tinyxml2::XMLDocument xmlDoc_;
    std::string filename_;
};

#endif // SESSIONPARSER_H

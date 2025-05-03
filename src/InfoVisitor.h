#ifndef INFOVISITOR_H
#define INFOVISITOR_H

#include "Visitor.h"
#include <cstdint>

class InfoVisitor : public Visitor
{
    std::string information_;
    bool brief_;
    uint64_t current_id_;

public:
    InfoVisitor();
    inline void setBriefStringMode () { brief_ = true; current_id_ = 0; }
    inline void setExtendedStringMode () { brief_ = false; current_id_ = 0; }
    inline void reset () { current_id_ = 0; }
    inline std::string str () const { return information_; }

    // Elements of Scene
    void visit (Scene& n) override;
    void visit (Node& n) override;
    void visit (Group& n) override;
    void visit (Switch& n) override;
    void visit (Primitive& n) override;

    // Elements with attributes
    void visit (Stream& n) override;
    void visit (MediaPlayer& n) override;
    void visit (MediaSource& s) override;
    void visit (SessionFileSource& s) override;
    void visit (SessionGroupSource& s) override;
    void visit (RenderSource& s) override;
    void visit (CloneSource& s) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;
    void visit (ScreenCaptureSource& s) override;
    void visit (NetworkSource& s) override;
    void visit (MultiFileSource& s) override;
    void visit (GenericStreamSource& s) override;
    void visit (SrtReceiverSource& s) override;
    void visit (TextSource& s) override;
    void visit (ShaderSource& s) override;
};

#endif // INFOVISITOR_H

#ifndef COUNTVISITOR_H
#define COUNTVISITOR_H

#include "Visitor.h"

class CountVisitor : public Visitor
{
    uint num_source_;
    uint num_playable_;

public:
    CountVisitor();
    inline uint numSources () const { return num_source_; }
    inline uint numPlayableSources () const { return num_playable_; }

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
    void visit (StreamSource& s) override;
    void visit (SessionFileSource& s) override;
    void visit (SessionGroupSource& s) override;
    void visit (RenderSource& s) override;
    void visit (CloneSource& s) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;
    void visit (NetworkSource& s) override;
    void visit (MultiFileSource& s) override;
    void visit (GenericStreamSource& s) override;
    void visit (SrtReceiverSource& s) override;
    void visit (ShaderSource &) override;
};

#endif // INFOVISITOR_H

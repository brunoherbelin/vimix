#ifndef PICKINGVISITOR_H
#define PICKINGVISITOR_H

#include <glm/glm.hpp>
#include <vector>
#include <utility>

#include "Visitor.h"

/**
 * @brief The PickingVisitor class is used to
 * capture which objects  of a scene are located at the screen
 * coordinate. Typically at mouse cursor coordinates for
 * user interaction.
 *
 * Only a subset of interactive objects (surface and Decorations)
 * are interactive.
 */
class PickingVisitor: public Visitor
{
    bool force_;
    std::vector<glm::vec3> points_;
    glm::mat4 modelview_;
    std::vector< std::pair<Node *, glm::vec2> > nodes_;

public:

    PickingVisitor(glm::vec3 coordinates, bool force = false);
    PickingVisitor(glm::vec3 selectionstart, glm::vec3 selection_end, bool force = false);

    bool empty() const {return nodes_.empty(); }
    size_t count() const {return nodes_.size(); }
    std::pair<Node *, glm::vec2> back() const { return nodes_.back(); }
    std::vector< std::pair<Node *, glm::vec2> >::const_reverse_iterator rbegin() { return nodes_.rbegin(); }
    std::vector< std::pair<Node *, glm::vec2> >::const_reverse_iterator rend()   { return nodes_.rend(); }

    // Elements of Scene
    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Primitive& n) override;

    /**
     * @brief visit Surface : picking source rendering surface
     * @param n
     */
    void visit(Surface& n) override;
    /**
     * @brief visit Handles : picking grabbers of source in geometry view
     * @param n
     */
    void visit(Handles& n) override;
    /**
     * @brief visit Symbol for mixing view
     * @param n
     */
    void visit(Symbol& n) override;
    /**
     * @brief visit Glyph for mixing view
     * @param n
     */
    void visit(Character& n) override;
    /**
     * @brief visit Disk : picking grabber for mixing view
     * @param n
     */
    void visit(Disk& n) override;

};

#endif // PICKINGVISITOR_H

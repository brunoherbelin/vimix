#ifndef ANNOTATIONS_H
#define ANNOTATIONS_H

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Toolkit/DialogToolkit.h"

struct ImDrawList;

///
/// Annotation : graphical element drawn on top of the GUI (for documentation screenshots)
///
class Annotation
{
public:
    typedef enum {
        ARROW = 0,
        TEXT,
        FRAME,
        CIRCLE,
        INVALID
    } Type;

    Annotation(Type t = ARROW, glm::vec2 position = glm::vec2(0.f));

    Type type;
    // ARROW  : tail, head, curve control point
    // TEXT   : top-left corner
    // FRAME  : two opposite corners
    // CIRCLE : center, point on circle
    glm::vec2 p[3];
    std::string text;
    glm::vec4 color;
    float thickness;
    float size;
    int font;
    bool shadow;

    int  numPoints() const;
    void move(glm::vec2 delta);
    void draw(ImDrawList *dl, glm::vec2 offset = glm::vec2(0.f), bool as_shadow = false) const;
    std::string label() const;
};

///
/// Annotations : overlay of annotations, editor and load / save in XML
///
class Annotations
{
    // Private Constructor
    Annotations();
    Annotations(Annotations const& copy) = delete;
    Annotations& operator=(Annotations const& copy) = delete;

public:

    static Annotations& manager()
    {
        // The only instance
        static Annotations _instance;
        return _instance;
    }

    // menu entries for the Guru toolbox
    void Menu();
    // draw overlay, handles and editor (editing hidden while capturing)
    void Render(bool capturing);

    // annotation mode : screenshot [F9] saves PNG and XML
    inline bool active() const { return active_; }
    // open file dialog to select PNG filename for current screenshot
    void saveScreenshot();

    void add(Annotation::Type t);
    void clear();
    bool load(const std::string &filename);
    bool save(const std::string &filename) const;

private:

    std::vector<Annotation> items_;
    int  selected_;
    bool active_;
    bool edit_;

    DialogToolkit::OpenFileDialog *opendialog_;
    DialogToolkit::SaveFileDialog *savedialog_;

    void RenderHandles();
    void RenderEditor();
};

#endif // ANNOTATIONS_H

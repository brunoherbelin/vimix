#ifndef COCOA_TOOLKIT_H
#define COCOA_TOOLKIT_H

#ifndef APPLE
#error "This file is only meant to be compiled for Mac OS X targets"
#endif

namespace CocoaToolkit {

void * get_current_nsopengl_context();

};

#endif // COCOA_TOOLKIT_H

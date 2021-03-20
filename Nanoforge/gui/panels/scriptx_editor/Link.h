#pragma once
#include "common/Typedefs.h"
#include "Node.h"

struct Link
{
    i32 Id; //Link uid
    i32 Start; //Start attribute uid
    i32 End; //End attribute uid
    Node* StartNode; //Node that owns the start attribute
    Node* EndNode; //Node that owns the end attribute
};
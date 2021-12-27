#pragma once
#include "Common/Typedefs.h"
#include "common/String.h"

enum class XtblType : u32
{
    None,
    Element,
    String,
    Int,
    Float,
    Vector,
    Color,
    Selection,
    Flags,
    List,
    Filename,
    ComboElement,
    Reference,
    Grid,
    TableDescription,
    Flag,
    Unsupported
};

extern XtblType XtblTypeFromString(const string& value);
extern string to_string(XtblType value);
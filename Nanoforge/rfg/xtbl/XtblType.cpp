#include "XtblType.h"
#include "Log.h"

#pragma warning(disable:4505)
XtblType XtblTypeFromString(const string& value)
{
    if (value == "Element")
        return XtblType::Element;
    else if (value == "String")
        return XtblType::String;
    else if (value == "Int")
        return XtblType::Int;
    else if (value == "Float")
        return XtblType::Float;
    else if (value == "Vector")
        return XtblType::Vector;
    else if (value == "Color")
        return XtblType::Color;
    else if (value == "Selection")
        return XtblType::Selection;
    else if (value == "Flags")
        return XtblType::Flags;
    else if (value == "List")
        return XtblType::List;
    else if (value == "Filename")
        return XtblType::Filename;
    else if (value == "ComboElement")
        return XtblType::ComboElement;
    else if (value == "Reference")
        return XtblType::Reference;
    else if (value == "Grid")
        //NOTE: Temporary fix for grid rendering issues. In many cases they are difficult to read an edit so for now they're drawn as lists
        //      The data for lists and grids is stored the same in the xtbl so it works just as well.
        return XtblType::List;
    else if (value == "TableDescription")
        return XtblType::TableDescription;
    else if (value == "Flag")
        return XtblType::Flag;
    else
        THROW_EXCEPTION("Invalid or unsupported string \"{}\".", value);
}

string to_string(XtblType value)
{
    switch (value)
    {
    case XtblType::Element:
        return "Element";
    case XtblType::String:
        return "String";
    case XtblType::Int:
        return "Int";
    case XtblType::Float:
        return "Float";
    case XtblType::Vector:
        return "Vector";
    case XtblType::Color:
        return "Color";
    case XtblType::Selection:
        return "Selection";
    case XtblType::Flags:
        return "Flags";
    case XtblType::List:
        return "List";
    case XtblType::Filename:
        return "Filename";
    case XtblType::ComboElement:
        return "ComboElement";
    case XtblType::Reference:
        return "Reference";
    case XtblType::Grid:
        return "Grid";
    case XtblType::TableDescription:
        return "TableDescription";
    case XtblType::Flag:
        return "Flag";
    case XtblType::Unsupported:
        return "Unsupported";
    default:
        THROW_EXCEPTION("Invalid or unsupported value for XtblType \"{}\".", value);
    }
}
#pragma warning(default:4505)
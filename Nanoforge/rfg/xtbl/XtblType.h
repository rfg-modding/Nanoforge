#pragma once
#include "Common/Typedefs.h"
#include "Log.h"

enum class XtblType : u32
{
    Element,
    String,
    Int,
    Float,
    Vector,
    Color,
    ColorLinear,
    Selection,
    Flags,
    List,
    Filename,
    ComboElement,
    Reference,
    GroupReference,
    Grid,
    TableDescription
};

static XtblType XtblTypeFromString(const string& value)
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
    else if (value == "ColorLinear")
        return XtblType::ColorLinear;
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
    else if (value == "GroupReference")
        return XtblType::GroupReference;
    else if (value == "Grid")
        return XtblType::Grid;
    else if (value == "TableDescription")
        return XtblType::TableDescription;
    else
        THROW_EXCEPTION("Invalid string \"{}\" passed to XtblTypeFromString", value);
}

static string to_string(XtblType value)
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
    case XtblType::ColorLinear:
        return "ColorLinear";
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
    case XtblType::GroupReference:
        return "GroupReference";
    case XtblType::Grid:
        return "Grid";
    case XtblType::TableDescription:
        return "TableDescription";
    default:
        THROW_EXCEPTION("Invalid value for XtblType \"{}\" passed to to_string in XtblType.h", value);
    }
}
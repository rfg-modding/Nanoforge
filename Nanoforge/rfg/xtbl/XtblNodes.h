#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "IXtblNode.h"
#include "XtblDescription.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>
#include <variant>

/*This file has all the IXtblNode implementations. One implementation per XtblType enum value.*/

//Not yet implemented:
//TableDescription,

class ElementXtblNode : public IXtblNode
{
public:
    ~ElementXtblNode()
    {
    
    }
    
    virtual void DrawEditor()
    {
        
    }
    
    virtual void InitDefault()
    {
    
    }
};

class StringXtblNode : public IXtblNode
{
public:
    ~StringXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {
        Value = "";
    }
};

class IntXtblNode : public IXtblNode
{
public:
    ~IntXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {
        Value = 0;
    }
};

class FloatXtblNode : public IXtblNode
{
public:
    ~FloatXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {
        Value = 0.0f;
    }
};

class VectorXtblNode : public IXtblNode
{
public:
    ~VectorXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {
        Value = Vec3(0.0f, 0.0f, 0.0f);
    }
};

class ColorXtblNode : public IXtblNode
{
public:
    ~ColorXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {
        Value = Vec3(0.0f, 0.0f, 0.0f);
    }
};

class SelectionXtblNode : public IXtblNode
{
public:
    ~SelectionXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {
        Value = "";
    }
};

class FlagsXtblNode : public IXtblNode
{
public:
    ~FlagsXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {

    }
};

class ListXtblNode : public IXtblNode
{
public:
    ~ListXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {

    }
};

class FilenameXtblNode : public IXtblNode
{
public:
    ~FilenameXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {
        Value = "";
    }
};

class ComboElementXtblNode : public IXtblNode
{
public:
    ~ComboElementXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {

    }
};

class ReferenceXtblNode : public IXtblNode
{
public:
    ~ReferenceXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {

    }
};

class GridXtblNode : public IXtblNode
{
public:
    ~GridXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {

    }
};

//Todo: Check if this is even used
class TableDescriptionXtblNode : public IXtblNode
{
public:
    ~TableDescriptionXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {

    }
};

class FlagXtblNode : public IXtblNode
{
public:
    ~FlagXtblNode()
    {

    }

    virtual void DrawEditor()
    {

    }

    virtual void InitDefault()
    {

    }
};

//Create node from XtblType using default value for that type
Handle<IXtblNode> CreateDefaultNode(XtblType type)
{
    switch (type)
    {
    case XtblType::Element:
        return CreateHandle<ElementXtblNode>();
    case XtblType::String:
        return CreateHandle<StringXtblNode>();
    case XtblType::Int:
        return CreateHandle<IntXtblNode>();
    case XtblType::Float:
        return CreateHandle<FloatXtblNode>();
    case XtblType::Vector:
        return CreateHandle<VectorXtblNode>();
    case XtblType::Color:
        return CreateHandle<ColorXtblNode>();
    case XtblType::Selection:
        return CreateHandle<SelectionXtblNode>();
    case XtblType::Flags:
        return CreateHandle<FlagsXtblNode>();
    case XtblType::List:
        return CreateHandle<ListXtblNode>();
    case XtblType::Filename:
        return CreateHandle<FilenameXtblNode>();
    case XtblType::ComboElement:
        return CreateHandle<ComboElementXtblNode>();
    case XtblType::Reference:
        return CreateHandle<ReferenceXtblNode>();
    case XtblType::Grid:
        return CreateHandle<GridXtblNode>();
    case XtblType::TableDescription:
        return CreateHandle<TableDescriptionXtblNode>();
    case XtblType::Flag:
        return CreateHandle<FlagXtblNode>();
    case XtblType::None:
    default:
        THROW_EXCEPTION("Invalid value for XtblType \"{}\" passed to CreateDefaultNode() in XtblNodes.h", type);
    }
}

//Create node from XtblDescription using default value from that description entry
Handle<IXtblNode> CreateDefaultNode(Handle<XtblDescription> desc)
{
    Handle<IXtblNode> node = nullptr;
    switch (desc->Type)
    {
    case XtblType::Element:
        node = CreateHandle<ElementXtblNode>();
        break;
    case XtblType::String:
        node = CreateHandle<StringXtblNode>();
        node->Value = desc->Default;
        break;
    case XtblType::Int:
        node = CreateHandle<IntXtblNode>();
        node->Value = std::stoi(desc->Default);
        break;
    case XtblType::Float:
        node = CreateHandle<FloatXtblNode>();
        node->Value = std::stof(desc->Default);
        break;
    case XtblType::Vector:
        node = CreateHandle<VectorXtblNode>();
        break;
    case XtblType::Color:
        node = CreateHandle<ColorXtblNode>();
        break;
    case XtblType::Selection:
        node = CreateHandle<SelectionXtblNode>();
        node->Value = desc->Default;
        break;
    case XtblType::Flags:
        node = CreateHandle<FlagsXtblNode>();
        break;
    case XtblType::List:
        node = CreateHandle<ListXtblNode>();
        break;
    case XtblType::Filename:
        node = CreateHandle<FilenameXtblNode>();
        node->Value = desc->Default;
        break;
    case XtblType::ComboElement:
        node = CreateHandle<ComboElementXtblNode>();
        break;
    case XtblType::Reference:
        node = CreateHandle<ReferenceXtblNode>();
        node->Value = desc->Default;
        break;
    case XtblType::Grid:
        node = CreateHandle<GridXtblNode>();
        break;
    case XtblType::TableDescription:
        node = CreateHandle<TableDescriptionXtblNode>();
        break;
    case XtblType::Flag:
        node = CreateHandle<FlagXtblNode>();
        break;
    case XtblType::None:
    default:
        THROW_EXCEPTION("Invalid value for XtblType \"{}\" passed to CreateDefaultNode() in XtblNodes.h", desc->Type);
    }

    return node;
}
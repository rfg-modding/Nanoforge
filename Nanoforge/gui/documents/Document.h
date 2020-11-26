#pragma once
#include "common/Typedefs.h"
#include <memory>

struct Document;
class GuiState;
//Function signature for document init and update functions
using DocumentInitFunction = void(GuiState* state, Handle<Document> doc);
using DocumentUpdateFunc = void(GuiState* state, Handle<Document> doc);
using DocumentOnCloseFunc = void(GuiState* state, Handle<Document> doc);

//Document instance. MainGui deletes the Data* you provide when the document is closed
struct Document
{
    Document(string title, DocumentInitFunction* init, DocumentUpdateFunc* update, DocumentOnCloseFunc* onClose, void* data = nullptr)
        : Title(title), Init(init), Update(update), OnClose(onClose), Data(data) {}

    string Title;
    DocumentInitFunction* Init = nullptr;
    DocumentUpdateFunc* Update = nullptr;
    DocumentOnCloseFunc* OnClose = nullptr;
    //Any data the document instance might need. Usually a heap allocated struct.
    void* Data = nullptr;
    bool Open = true;
    bool FirstDraw = true;
};
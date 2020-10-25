#pragma once
#include "common/Typedefs.h"

struct Document;
class GuiState;
//Function signature for document init and update functions
using DocumentInitFunction = void(GuiState* state, Document& doc);
using DocumentUpdateFunc = void(GuiState* state, Document& doc);

//Document instance. MainGui deletes the Data* you provide when the document is closed
struct Document
{
    Document(string title, DocumentInitFunction* init, DocumentUpdateFunc* update, void* data = nullptr)
        : Title(title), Init(init), Update(update), Data(data) {}

    string Title;
    DocumentInitFunction* Init = nullptr;
    DocumentUpdateFunc* Update = nullptr;
    void* Data = nullptr;
    bool Open = true;
};
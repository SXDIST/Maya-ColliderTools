#pragma once

#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class ColliderCreateCmd : public MPxCommand
{
public:
    static const char* commandName;

    static void* creator();
    static MSyntax syntax();

    MStatus doIt(const MArgList& args) override;
    bool isUndoable() const override;
};

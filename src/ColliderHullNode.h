#pragma once

#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

class ColliderHullNode : public MPxNode
{
public:
    static const MTypeId id;
    static const char* typeName;

    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static MObject useSkinWidthAttr;
    static MObject skinWidthAttr;
    static MObject normalEpsilonAttr;
    static MObject useTrianglesAttr;
    static MObject maxOutputVerticesAttr;
    static MObject useReverseTriOrderAttr;
    static MObject inputAttr;
    static MObject inputComponentsAttr;
    static MObject inputPolymeshAttr;
    static MObject outputPolymeshAttr;

private:
    MStatus processInputIndex(MPointArray& allPoints, MDataHandle& meshHndl, MDataHandle& compHndl);
};

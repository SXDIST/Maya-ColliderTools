#pragma once

#include <maya/MObject.h>
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

class ColliderPrimitiveNode : public MPxNode
{
public:
    static const MTypeId id;
    static const char* typeName;

    static void* creator();
    static MStatus initialize();

    MStatus compute(const MPlug& plug, MDataBlock& data) override;

    static MObject inputMeshAttr;
    static MObject inputComponentsAttr;
    static MObject colliderTypeAttr;
    static MObject segmentsAttr;
    static MObject radiusScaleAttr;
    static MObject lengthScaleAttr;
    static MObject outputMeshAttr;
};

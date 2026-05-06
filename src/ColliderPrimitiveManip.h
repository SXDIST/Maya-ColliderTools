#pragma once

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPxManipContainer.h>
#include <maya/MTypeId.h>

class ColliderPrimitiveManip : public MPxManipContainer
{
public:
    static const MTypeId id;
    static const char* typeName;

    static void* creator();
    static MStatus initialize();

    MStatus createChildren() override;
    MStatus connectToDependNode(const MObject& node) override;
    void draw(M3dView& view, const MDagPath& path, M3dView::DisplayStyle style, M3dView::DisplayStatus status) override;

private:
    MDagPath radiusManipPath;
    MDagPath lengthManipPath;
};

#include "ColliderPrimitiveManip.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnDistanceManip.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>

const MTypeId ColliderPrimitiveManip::id(0x0012b002);
const char* ColliderPrimitiveManip::typeName = "ctColliderPrimitiveManip";

void* ColliderPrimitiveManip::creator()
{
    return new ColliderPrimitiveManip;
}

MStatus ColliderPrimitiveManip::initialize()
{
    return MPxManipContainer::initialize();
}

MStatus ColliderPrimitiveManip::createChildren()
{
    radiusManipPath = addDistanceManip("radiusScaleManip", "radiusScale");
    MFnDistanceManip radiusManip(radiusManipPath);
    radiusManip.setStartPoint(MPoint(0.0, 0.0, 0.0));
    radiusManip.setDirection(MVector(1.0, 0.0, 0.0));
    radiusManip.setDrawStart(true);
    radiusManip.setDrawLine(true);

    lengthManipPath = addDistanceManip("lengthScaleManip", "lengthScale");
    MFnDistanceManip lengthManip(lengthManipPath);
    lengthManip.setStartPoint(MPoint(0.0, 0.0, 0.0));
    lengthManip.setDirection(MVector(0.0, 1.0, 0.0));
    lengthManip.setDrawStart(true);
    lengthManip.setDrawLine(true);

    return MStatus::kSuccess;
}

MStatus ColliderPrimitiveManip::connectToDependNode(const MObject& node)
{
    MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status)
    {
        return status;
    }

    MPlug radiusPlug = nodeFn.findPlug("radiusScale", true, &status);
    if (!status)
    {
        return status;
    }
    MFnDistanceManip radiusManip(radiusManipPath);
    status = radiusManip.connectToDistancePlug(radiusPlug);
    if (!status)
    {
        return status;
    }
    addPlugToInViewEditor(radiusPlug);

    MPlug lengthPlug = nodeFn.findPlug("lengthScale", true, &status);
    if (!status)
    {
        return status;
    }
    MFnDistanceManip lengthManip(lengthManipPath);
    status = lengthManip.connectToDistancePlug(lengthPlug);
    if (!status)
    {
        return status;
    }
    addPlugToInViewEditor(lengthPlug);

    finishAddingManips();
    return MPxManipContainer::connectToDependNode(node);
}

void ColliderPrimitiveManip::draw(M3dView& view, const MDagPath& path, M3dView::DisplayStyle style, M3dView::DisplayStatus status)
{
    MPxManipContainer::draw(view, path, style, status);
}

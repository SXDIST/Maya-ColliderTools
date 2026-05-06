#pragma once

#include "ColliderGeometry.h"

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MVector.h>
#include <vector>

namespace ColliderTools
{
struct PointSet
{
    MDagPath sourcePath;
    MObject component;
    MPointArray points;
};

struct BoxFit
{
    MVector center;
    MVector axes[3];
    double halfExtents[3];
};

MStatus gatherSelectionPointSets(const MSelectionList& selection, std::vector<PointSet>& pointSets);
MStatus gatherCombinedSelectionPoints(const MSelectionList& selection, MPointArray& points);
MPointArray cleanPointSet(const MPointArray& points);

BoxFit fitAabb(const MPointArray& points);
BoxFit fitObb(const MPointArray& points);
void fitAxisShape(const MPointArray& points, const MVector axes[3], MVector& center, MVector& axis, double& halfLength, double& radius);
}

#pragma once

#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <vector>

namespace ColliderTools
{
struct MeshData
{
    MPointArray points;
    MIntArray faceCounts;
    MIntArray faceConnects;
};

MStatus assignInitialShadingGroup(const MObject& shape);
MStatus hardenMeshEdges(const MObject& shape);
MStatus createMeshTransform(const MeshData& mesh, const MString& name, MObject& transform, MObject& shape);
void orientFacesOutward(MeshData& mesh);

MeshData makeBoxMesh(const MVector& center, const MVector axes[3], const double halfExtents[3]);
MeshData makeCylinderMesh(const MVector& center, const MVector& axis, double halfLength, double radius, int segments);
MeshData makeCapsuleMesh(const MVector& center, const MVector& axis, double halfLength, double radius, int segments);

MVector safeNormal(const MVector& value, const MVector& fallback);
void buildFrame(const MVector& axis, MVector& right, MVector& up, MVector& forward);
}

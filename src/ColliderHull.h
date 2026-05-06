#pragma once

#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>

namespace ColliderTools
{
struct HullOptions
{
    bool forceTriangles = true;
    int maxOutputVertices = 4096;
    bool useSkinWidth = false;
    double skinWidth = 0.01;
    double normalEpsilon = 0.001;
    bool reverseTriangleOrder = false;
};

MStatus createHullMeshData(MObject& output, const MPointArray& points, const HullOptions& options);
MStatus createHullMeshData(MObject& output, const MObject& inputMesh, const HullOptions& options);
MStatus componentToVertexIds(MIntArray& outIndices, const MObject& mesh, const MObject& component);
}

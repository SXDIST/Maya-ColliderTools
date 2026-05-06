#include "ColliderHull.h"

#include "ColliderHullCore.h"

#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ColliderTools
{
namespace
{
using HullCore::HullMesh;
using HullCore::HullRequest;
constexpr int kMinHullVertices = 4;
constexpr int kMaxHullVertices = 4096;

int clampMaxVertices(int value)
{
    return std::max(kMinHullVertices, std::min(value, kMaxHullVertices));
}

bool isFinitePoint(const MPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool isFiniteOptions(const HullOptions& options)
{
    return std::isfinite(options.skinWidth) && std::isfinite(options.normalEpsilon) && options.normalEpsilon > 0.0;
}

bool hasHullVolume(const MPointArray& points)
{
    MPoint minPoint(points[0]);
    MPoint maxPoint(points[0]);
    for (unsigned int i = 1; i < points.length(); ++i)
    {
        const MPoint& point = points[i];
        minPoint.x = std::min(minPoint.x, point.x);
        minPoint.y = std::min(minPoint.y, point.y);
        minPoint.z = std::min(minPoint.z, point.z);
        maxPoint.x = std::max(maxPoint.x, point.x);
        maxPoint.y = std::max(maxPoint.y, point.y);
        maxPoint.z = std::max(maxPoint.z, point.z);
    }

    const MVector diagonal = maxPoint - minPoint;
    const double scale = diagonal.length();
    if (scale <= 1.0e-8)
    {
        return false;
    }

    const double distanceEpsilon = scale * 1.0e-7;
    const double areaEpsilon = scale * scale * 1.0e-12;
    const double volumeEpsilon = scale * scale * scale * 1.0e-12;

    unsigned int secondIndex = 0;
    for (unsigned int i = 1; i < points.length(); ++i)
    {
        if ((points[i] - points[0]).length() > distanceEpsilon)
        {
            secondIndex = i;
            break;
        }
    }
    if (secondIndex == 0)
    {
        return false;
    }

    unsigned int thirdIndex = 0;
    MVector normal;
    const MVector base = points[secondIndex] - points[0];
    for (unsigned int i = 1; i < points.length(); ++i)
    {
        if (i == secondIndex)
        {
            continue;
        }

        normal = base ^ (points[i] - points[0]);
        if (normal.length() > areaEpsilon)
        {
            thirdIndex = i;
            break;
        }
    }
    if (thirdIndex == 0)
    {
        return false;
    }

    for (unsigned int i = 1; i < points.length(); ++i)
    {
        if (i == secondIndex || i == thirdIndex)
        {
            continue;
        }

        const double volume = std::abs(normal * (points[i] - points[0]));
        if (volume > volumeEpsilon)
        {
            return true;
        }
    }
    return false;
}

bool appendPolygonResult(const HullMesh& hullMesh, MIntArray& polyCounts, MIntArray& vertexConnects)
{
    unsigned int consumed = 0;
    for (unsigned int i = 0; i < hullMesh.faceCount; ++i)
    {
        if (consumed >= hullMesh.indices.size())
        {
            return false;
        }

        const unsigned int count = hullMesh.indices[consumed++];
        if (count < 3 || count > hullMesh.vertexCount || consumed + count > hullMesh.indices.size())
        {
            return false;
        }

        polyCounts.append(count);
        for (unsigned int j = 0; j < count; ++j)
        {
            const unsigned int value = hullMesh.indices[consumed++];
            if (value >= hullMesh.vertexCount)
            {
                return false;
            }
            vertexConnects.append(value);
        }
    }
    return consumed == hullMesh.indices.size();
}

bool appendTriangleResult(const HullMesh& hullMesh, MIntArray& polyCounts, MIntArray& vertexConnects)
{
    if (hullMesh.indices.size() < hullMesh.faceCount * 3)
    {
        return false;
    }

    polyCounts.setLength(hullMesh.faceCount);
    for (unsigned int i = 0; i < hullMesh.faceCount; ++i)
    {
        polyCounts[i] = 3;
        const unsigned int offset = i * 3;
        for (unsigned int j = 0; j < 3; ++j)
        {
            const unsigned int value = hullMesh.indices[offset + j];
            if (value >= hullMesh.vertexCount)
            {
                return false;
            }
            vertexConnects.append(value);
        }
    }
    return true;
}
}

MStatus createHullMeshData(MObject& output, const MObject& inputMesh, const HullOptions& options)
{
    if (!inputMesh.hasFn(MFn::kMesh))
    {
        return MStatus::kInvalidParameter;
    }

    MStatus status;
    MFnMesh meshFn(inputMesh, &status);
    if (!status)
    {
        return status;
    }

    MPointArray points;
    status = meshFn.getPoints(points);
    if (!status)
    {
        return status;
    }
    return createHullMeshData(output, points, options);
}

MStatus createHullMeshData(MObject& output, const MPointArray& points, const HullOptions& options)
{
    if (points.length() < kMinHullVertices || !isFiniteOptions(options))
    {
        return MStatus::kInvalidParameter;
    }
    if (!hasHullVolume(points))
    {
        return MStatus::kInvalidParameter;
    }

    std::vector<double> inputVerts;
    inputVerts.reserve(points.length() * 3);
    for (unsigned int i = 0; i < points.length(); ++i)
    {
        const MPoint& point = points[i];
        if (!isFinitePoint(point))
        {
            return MStatus::kInvalidParameter;
        }
        inputVerts.push_back(point.x);
        inputVerts.push_back(point.y);
        inputVerts.push_back(point.z);
    }

    HullRequest hullRequest;
    hullRequest.vertices = inputVerts.data();
    hullRequest.vertexCount = points.length();
    hullRequest.vertexStride = sizeof(double) * 3;
    hullRequest.maxVertices = clampMaxVertices(options.maxOutputVertices);
    hullRequest.skinWidth = options.skinWidth;
    hullRequest.normalEpsilon = options.normalEpsilon;
    hullRequest.forceTriangles = options.forceTriangles;
    hullRequest.useSkinWidth = options.useSkinWidth;
    hullRequest.reverseTriangleOrder = options.reverseTriangleOrder;

    HullMesh hullMesh;
    if (!HullCore::computeHull(hullRequest, hullMesh) || hullMesh.vertexCount < kMinHullVertices || hullMesh.faceCount < 4 || hullMesh.vertices.empty() || hullMesh.indices.empty())
    {
        return MStatus::kFailure;
    }

    MPointArray outPoints;
    for (unsigned int i = 0; i < hullMesh.vertexCount; ++i)
    {
        const unsigned int offset = i * 3;
        const MPoint point(hullMesh.vertices[offset], hullMesh.vertices[offset + 1], hullMesh.vertices[offset + 2]);
        if (!isFinitePoint(point))
        {
            return MStatus::kFailure;
        }
        outPoints.append(point);
    }

    MIntArray polyCounts;
    MIntArray vertexConnects;
    const bool validIndices = hullMesh.polygons
        ? appendPolygonResult(hullMesh, polyCounts, vertexConnects)
        : appendTriangleResult(hullMesh, polyCounts, vertexConnects);
    if (!validIndices || polyCounts.length() == 0 || vertexConnects.length() == 0)
    {
        return MStatus::kFailure;
    }

    MStatus status;
    MFnMesh meshFn(output, &status);
    if (!status)
    {
        return status;
    }

    meshFn.create(hullMesh.vertexCount, hullMesh.faceCount, outPoints, polyCounts, vertexConnects, output, &status);
    return status;
}

MStatus componentToVertexIds(MIntArray& outIndices, const MObject& mesh, const MObject& component)
{
    MStatus status;
    MFnSingleIndexedComponent compFn(component, &status);
    if (!status)
    {
        return status;
    }

    MFnMesh meshFn(mesh, &status);
    if (!status)
    {
        return status;
    }

    MIntArray elements;
    status = compFn.getElements(elements);
    if (!status)
    {
        return status;
    }

    const unsigned int elementCount = elements.length();
    const MFn::Type componentType = compFn.componentType();
    if (componentType == MFn::kMeshVertComponent)
    {
        outIndices = elements;
    }
    else if (componentType == MFn::kMeshEdgeComponent)
    {
        for (unsigned int i = 0; i < elementCount; ++i)
        {
            int2 edgeVerts;
            status = meshFn.getEdgeVertices(elements[i], edgeVerts);
            if (!status)
            {
                return status;
            }
            outIndices.append(edgeVerts[0]);
            outIndices.append(edgeVerts[1]);
        }
    }
    else if (componentType == MFn::kMeshPolygonComponent)
    {
        for (unsigned int i = 0; i < elementCount; ++i)
        {
            MIntArray polyVerts;
            status = meshFn.getPolygonVertices(elements[i], polyVerts);
            if (!status)
            {
                return status;
            }
            for (unsigned int j = 0; j < polyVerts.length(); ++j)
            {
                outIndices.append(polyVerts[j]);
            }
        }
    }
    else if (componentType == MFn::kMeshFaceVertComponent)
    {
        MIntArray faceCounts;
        MIntArray faceVerts;
        status = meshFn.getVertices(faceCounts, faceVerts);
        if (!status)
        {
            return status;
        }
        for (unsigned int j = 0; j < elementCount && j < faceVerts.length(); ++j)
        {
            outIndices.append(faceVerts[j]);
        }
    }
    else
    {
        return MStatus::kNotImplemented;
    }
    return MStatus::kSuccess;
}
}

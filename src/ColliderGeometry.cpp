#include "ColliderGeometry.h"

#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ColliderTools
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

void appendQuad(MeshData& mesh, int a, int b, int c, int d)
{
    mesh.faceCounts.append(4);
    mesh.faceConnects.append(a);
    mesh.faceConnects.append(b);
    mesh.faceConnects.append(c);
    mesh.faceConnects.append(d);
}

void appendTri(MeshData& mesh, int a, int b, int c)
{
    mesh.faceCounts.append(3);
    mesh.faceConnects.append(a);
    mesh.faceConnects.append(b);
    mesh.faceConnects.append(c);
}

MPoint toPoint(const MVector& value)
{
    return MPoint(value.x, value.y, value.z);
}

void setBoolPlug(MFnDagNode& nodeFn, const char* name, bool value)
{
    MStatus status;
    MPlug plug = nodeFn.findPlug(name, true, &status);
    if (status && !plug.isNull())
    {
        plug.setBool(value);
    }
}

void setIntPlug(MFnDagNode& nodeFn, const char* name, int value)
{
    MStatus status;
    MPlug plug = nodeFn.findPlug(name, true, &status);
    if (status && !plug.isNull())
    {
        plug.setInt(value);
    }
}

MVector meshCenter(const MPointArray& points)
{
    MVector center;
    if (points.length() == 0)
    {
        return center;
    }

    for (unsigned int i = 0; i < points.length(); ++i)
    {
        center += MVector(points[i]);
    }
    return center / static_cast<double>(points.length());
}

MVector faceNormal(const MeshData& mesh, int start, int count)
{
    if (count < 3)
    {
        return MVector::zero;
    }

    const MVector first(mesh.points[mesh.faceConnects[start]]);
    for (int i = 1; i + 1 < count; ++i)
    {
        const MVector second(mesh.points[mesh.faceConnects[start + i]]);
        const MVector third(mesh.points[mesh.faceConnects[start + i + 1]]);
        const MVector normal = (second - first) ^ (third - first);
        if (normal.length() > 1.0e-8)
        {
            return normal;
        }
    }
    return MVector::zero;
}

void reverseFace(MeshData& mesh, int start, int count)
{
    for (int i = 0; i < count / 2; ++i)
    {
        const int left = start + i;
        const int right = start + count - 1 - i;
        const int value = mesh.faceConnects[left];
        mesh.faceConnects.set(mesh.faceConnects[right], left);
        mesh.faceConnects.set(value, right);
    }
}
}

MVector safeNormal(const MVector& value, const MVector& fallback)
{
    if (value.length() > 1.0e-8)
    {
        MVector result(value);
        result.normalize();
        return result;
    }
    return fallback;
}

void buildFrame(const MVector& axis, MVector& right, MVector& up, MVector& forward)
{
    forward = safeNormal(axis, MVector::zAxis);
    MVector reference = std::abs(forward * MVector::yAxis) < 0.95 ? MVector::yAxis : MVector::xAxis;
    right = safeNormal(reference ^ forward, MVector::xAxis);
    up = safeNormal(forward ^ right, MVector::yAxis);
}

MStatus assignInitialShadingGroup(const MObject& shape)
{
    MStatus status;
    MFnDagNode shapeFn(shape, &status);
    if (!status)
    {
        return status;
    }

    setBoolPlug(shapeFn, "overrideEnabled", false);
    setBoolPlug(shapeFn, "overrideRGBColors", false);
    setBoolPlug(shapeFn, "overrideShading", true);
    setBoolPlug(shapeFn, "displayColors", false);
    setBoolPlug(shapeFn, "useObjectColor", false);
    setIntPlug(shapeFn, "objectColor", 0);
    setIntPlug(shapeFn, "overrideColor", 0);

    MObject parent = shapeFn.parent(0, &status);
    if (status && !parent.isNull())
    {
        MFnDagNode parentFn(parent, &status);
        if (status)
        {
            setBoolPlug(parentFn, "overrideEnabled", false);
            setBoolPlug(parentFn, "overrideRGBColors", false);
            setBoolPlug(parentFn, "overrideShading", true);
            setIntPlug(parentFn, "overrideColor", 0);
        }
    }

    MSelectionList selection;
    status = selection.add("initialShadingGroup");
    if (!status)
    {
        return MStatus::kSuccess;
    }

    MObject shadingGroup;
    status = selection.getDependNode(0, shadingGroup);
    if (!status || shadingGroup.isNull())
    {
        return MStatus::kSuccess;
    }

    MFnSet shadingSet(shadingGroup, &status);
    if (!status)
    {
        return MStatus::kSuccess;
    }

    shadingSet.addMember(shape);
    return MStatus::kSuccess;
}

MStatus hardenMeshEdges(const MObject& shape)
{
    MStatus status;
    MFnMesh meshFn(shape, &status);
    if (!status)
    {
        return status;
    }

    const int edgeCount = meshFn.numEdges(&status);
    if (!status)
    {
        return status;
    }
    for (int edge = 0; edge < edgeCount; ++edge)
    {
        meshFn.setEdgeSmoothing(edge, false);
    }
    meshFn.cleanupEdgeSmoothing();
    return meshFn.updateSurface();
}

void orientFacesOutward(MeshData& mesh)
{
    const MVector center = meshCenter(mesh.points);
    int start = 0;
    for (unsigned int face = 0; face < mesh.faceCounts.length(); ++face)
    {
        const int count = mesh.faceCounts[face];
        if (count >= 3)
        {
            MVector faceCenter;
            for (int i = 0; i < count; ++i)
            {
                faceCenter += MVector(mesh.points[mesh.faceConnects[start + i]]);
            }
            faceCenter /= static_cast<double>(count);

            const MVector normal = faceNormal(mesh, start, count);
            if (normal.length() > 1.0e-8 && normal * (faceCenter - center) < 0.0)
            {
                reverseFace(mesh, start, count);
            }
        }
        start += count;
    }
}

MStatus createMeshTransform(const MeshData& mesh, const MString& name, MObject& transform, MObject& shape)
{
    MeshData orientedMesh = mesh;
    orientFacesOutward(orientedMesh);

    MStatus status;
    MFnTransform transformFn;
    transform = transformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return status;
    }

    if (name.length() > 0)
    {
        transformFn.setName(name, false, &status);
        if (!status)
        {
            return status;
        }
    }

    MFnMesh meshFn;
    shape = meshFn.create(orientedMesh.points.length(), orientedMesh.faceCounts.length(), orientedMesh.points, orientedMesh.faceCounts, orientedMesh.faceConnects, transform, &status);
    if (!status)
    {
        return status;
    }

    MFnDagNode shapeFn(shape, &status);
    if (status)
    {
        shapeFn.setName(MString(transformFn.name()) + "Shape", false, &status);
    }

    hardenMeshEdges(shape);
    status = assignInitialShadingGroup(shape);
    return status;
}

MeshData makeBoxMesh(const MVector& center, const MVector axes[3], const double halfExtents[3])
{
    MeshData mesh;
    const int signs[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
    };

    for (int i = 0; i < 8; ++i)
    {
        MVector point = center;
        for (int axis = 0; axis < 3; ++axis)
        {
            point += axes[axis] * halfExtents[axis] * signs[i][axis];
        }
        mesh.points.append(toPoint(point));
    }

    appendQuad(mesh, 0, 1, 2, 3);
    appendQuad(mesh, 4, 7, 6, 5);
    appendQuad(mesh, 0, 4, 5, 1);
    appendQuad(mesh, 1, 5, 6, 2);
    appendQuad(mesh, 2, 6, 7, 3);
    appendQuad(mesh, 3, 7, 4, 0);
    return mesh;
}

MeshData makeCylinderMesh(const MVector& center, const MVector& axis, double halfLength, double radius, int segments)
{
    MeshData mesh;
    segments = std::max(segments, 6);
    radius = std::max(radius, 1.0e-4);
    halfLength = std::max(halfLength, 1.0e-4);

    MVector right, up, forward;
    buildFrame(axis, right, up, forward);
    const MVector bottomCenter = center - forward * halfLength;
    const MVector topCenter = center + forward * halfLength;

    for (int i = 0; i < segments; ++i)
    {
        const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(segments);
        const MVector radial = right * std::cos(angle) + up * std::sin(angle);
        mesh.points.append(toPoint(bottomCenter + radial * radius));
        mesh.points.append(toPoint(topCenter + radial * radius));
    }
    const int bottomIndex = static_cast<int>(mesh.points.length());
    mesh.points.append(toPoint(bottomCenter));
    const int topIndex = static_cast<int>(mesh.points.length());
    mesh.points.append(toPoint(topCenter));

    for (int i = 0; i < segments; ++i)
    {
        const int next = (i + 1) % segments;
        appendQuad(mesh, i * 2, next * 2, next * 2 + 1, i * 2 + 1);
        appendTri(mesh, bottomIndex, next * 2, i * 2);
        appendTri(mesh, topIndex, i * 2 + 1, next * 2 + 1);
    }
    return mesh;
}

MeshData makeCapsuleMesh(const MVector& center, const MVector& axis, double halfLength, double radius, int segments)
{
    MeshData mesh;
    segments = std::max(segments, 8);
    const int ringsPerHemisphere = std::max(2, segments / 4);
    radius = std::max(radius, 1.0e-4);
    halfLength = std::max(halfLength - radius, 0.0);

    MVector right, up, forward;
    buildFrame(axis, right, up, forward);
    const MVector bottomCenter = center - forward * halfLength;
    const MVector topCenter = center + forward * halfLength;
    std::vector<int> ringStarts;

    const int bottomPole = static_cast<int>(mesh.points.length());
    mesh.points.append(toPoint(bottomCenter - forward * radius));

    for (int ring = 1; ring <= ringsPerHemisphere; ++ring)
    {
        const double theta = -kPi * 0.5 + (kPi * 0.5 * ring / ringsPerHemisphere);
        const double z = std::sin(theta) * radius;
        const double r = std::cos(theta) * radius;
        ringStarts.push_back(static_cast<int>(mesh.points.length()));
        for (int i = 0; i < segments; ++i)
        {
            const double angle = 2.0 * kPi * i / segments;
            const MVector radial = right * std::cos(angle) + up * std::sin(angle);
            mesh.points.append(toPoint(bottomCenter + forward * z + radial * r));
        }
    }

    for (int ring = 1; ring <= ringsPerHemisphere; ++ring)
    {
        const double theta = kPi * 0.5 * ring / ringsPerHemisphere;
        const double z = std::sin(theta) * radius;
        const double r = std::cos(theta) * radius;
        if (ring == ringsPerHemisphere)
        {
            break;
        }
        ringStarts.push_back(static_cast<int>(mesh.points.length()));
        for (int i = 0; i < segments; ++i)
        {
            const double angle = 2.0 * kPi * i / segments;
            const MVector radial = right * std::cos(angle) + up * std::sin(angle);
            mesh.points.append(toPoint(topCenter + forward * z + radial * r));
        }
    }

    const int topPole = static_cast<int>(mesh.points.length());
    mesh.points.append(toPoint(topCenter + forward * radius));

    if (!ringStarts.empty())
    {
        const int firstRing = ringStarts.front();
        for (int i = 0; i < segments; ++i)
        {
            appendTri(mesh, bottomPole, firstRing + i, firstRing + ((i + 1) % segments));
        }

        for (size_t ring = 0; ring + 1 < ringStarts.size(); ++ring)
        {
            const int a = ringStarts[ring];
            const int b = ringStarts[ring + 1];
            for (int i = 0; i < segments; ++i)
            {
                appendQuad(mesh, a + i, a + ((i + 1) % segments), b + ((i + 1) % segments), b + i);
            }
        }

        const int lastRing = ringStarts.back();
        for (int i = 0; i < segments; ++i)
        {
            appendTri(mesh, topPole, lastRing + ((i + 1) % segments), lastRing + i);
        }
    }
    return mesh;
}
}

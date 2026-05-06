//
// DDConvexHullUtils.cpp
// DDConvexHull
//
// Created by Jonathan Tilden on 1/28/13.
//
// MIT License
//
// Copyright (c) 2017 Jonathan Tilden

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "DDConvexHullUtils.h"
#include "StanHull/hull.h"
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MIntArray.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <algorithm>
#include <cmath>
#include <sstream>
MString DDConvexHullUtils::dbl_to_string(double x)
{
    std::ostringstream ss;
    ss << x;
    return MString((ss.str()).c_str());
}

MString DDConvexHullUtils::int_to_string(int x)
{
    std::ostringstream ss;
    ss << x;
    return MString((ss.str()).c_str());
}

MStatus DDConvexHullUtils::generateMayaHull(MObject &output,
                                const MObject &input,
                                const DDConvexHullUtils::hullOpts &hullOptions)
{
    // Convert the input mobject to mfnmesh
    if (!input.hasFn(MFn::kMesh))
    {
        return MStatus::kInvalidParameter;
    }
    MFnMesh inputMesh(input);
    
    // Allocate an array for the vertices and fill it with the data extracted
    // from the input mesh
    uint numInputVerts = inputMesh.numVertices();
    if (!numInputVerts)
    {
        return MStatus::kFailure;
    }
    
    MPointArray points;
    inputMesh.getPoints(points);
    
    return generateMayaHull(output, points, hullOptions);
}

MStatus DDConvexHullUtils::generateMayaHull(MObject &output,
                                const MPointArray &vertices,
                                const DDConvexHullUtils::hullOpts &hullOptions)
{
    // Allocate and push the vert list into the new array Mem Cleanup req.
    uint numInputVerts = vertices.length();
    if (numInputVerts < 4)
    {
        return MStatus::kInvalidParameter;
    }

    double *inputVerts = new double[numInputVerts*3];
    for (uint i=0; i < numInputVerts; i++)
    {
        const MPoint& point = vertices[i];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
        {
            delete[] inputVerts;
            return MStatus::kInvalidParameter;
        }

        uint offset = i*3;
        inputVerts[offset]   = point.x;
        inputVerts[offset+1] = point.y;
        inputVerts[offset+2] = point.z;
    }
    
    // Setup the flags
    uint hullFlags = QF_DEFAULT;
    if (hullOptions.forceTriangles)
    {
        hullFlags |= QF_TRIANGLES;
    }
    if (hullOptions.useSkinWidth)
    {
        hullFlags |= QF_SKIN_WIDTH;
    }
    if (hullOptions.reverseTriangleOrder)
    {
        hullFlags |= QF_REVERSE_ORDER;
    }
    
    // Create the description
    HullDesc hullDescription;
    hullDescription.mFlags = hullFlags;
    hullDescription.mMaxVertices = hullOptions.maxOutputVertices;
    hullDescription.mSkinWidth = hullOptions.skinWidth;
    hullDescription.mNormalEpsilon = hullOptions.normalEpsilon;
    hullDescription.mVertexStride = sizeof(double)*3;
    hullDescription.mVcount = numInputVerts;
    hullDescription.mVertices = inputVerts;
    
    // Create the hull
    HullLibrary hullComputer;
    HullResult hullResult;
    HullError err = hullComputer.CreateConvexHull(hullDescription, hullResult);
    MStatus hullStat = MStatus::kSuccess;
    if (err == QE_OK)
    {
        if (hullResult.mNumOutputVertices < 4 || hullResult.mNumFaces < 4 || !hullResult.mOutputVertices || !hullResult.mIndices)
        {
            hullStat = MStatus::kFailure;
        }
        else
        {
            // Grab the verts
            MPointArray outPoints;
            bool validResult = true;
            for (uint i=0; i < hullResult.mNumOutputVertices; i++)
            {
                uint offset = i*3;
                MPoint curPoint(hullResult.mOutputVertices[offset],
                                hullResult.mOutputVertices[offset+1],
                                hullResult.mOutputVertices[offset+2]);
                if (!std::isfinite(curPoint.x) || !std::isfinite(curPoint.y) || !std::isfinite(curPoint.z))
                {
                    validResult = false;
                    break;
                }
                outPoints.append(curPoint);
            }

            // Check if the results are in polygons, or triangles. Depending on
            // which for the result is in, the way the face indices are setup
            // is different.
            MIntArray polyCounts;
            MIntArray vertexConnects;

            if (validResult && hullResult.mPolygons)
            {
                const uint *idx = hullResult.mIndices;
                for (uint i=0; i < hullResult.mNumFaces && validResult; i++)
                {
                    uint pCount = *idx++;
                    if (pCount < 3 || pCount > hullResult.mNumOutputVertices)
                    {
                        validResult = false;
                        break;
                    }
                    polyCounts.append(pCount);

                    for (uint j=0; j < pCount; j++)
                    {
                        uint val = idx[0];
                        if (val >= hullResult.mNumOutputVertices)
                        {
                            validResult = false;
                            break;
                        }
                        vertexConnects.append(val);
                        idx++;
                    }
                }
            }
            else if (validResult)
            {
                polyCounts.setLength(hullResult.mNumFaces);
                for (uint i=0; i < hullResult.mNumFaces && validResult; i++)
                {
                    polyCounts[i] = 3;
                    uint *idx = &hullResult.mIndices[i*3];
                    for (uint j=0; j < 3; ++j)
                    {
                        if (idx[j] >= hullResult.mNumOutputVertices)
                        {
                            validResult = false;
                            break;
                        }
                        vertexConnects.append(idx[j]);
                    }
                }
            }

            if (!validResult || polyCounts.length() == 0 || vertexConnects.length() == 0)
            {
                hullStat = MStatus::kFailure;
            }
            else
            {
                // Setup the outmesh
                MFnMesh outMeshFn(output);
                outMeshFn.create(hullResult.mNumOutputVertices,
                                 hullResult.mNumFaces,
                                 outPoints,
                                 polyCounts,
                                 vertexConnects,
                                 output,
                                 &hullStat);
            }
        }
    }
    else
    {
        hullStat = MStatus::kFailure;
    }
    
    // Mem Cleanup
    hullComputer.ReleaseResult(hullResult);
    delete[] inputVerts;
    
    return hullStat;
}

MStatus DDConvexHullUtils::componentToVertexIDs(MIntArray &outIndices,
                                                const MObject &mesh,
                                                const MObject &component)
{
    // Create the funciton sets
    MFnSingleIndexedComponent compFn(component);
    MFnMesh meshFn(mesh);
        
    MIntArray elems;
    compFn.getElements(elems);
    uint elemLen = elems.length();
    
    // Convert the components to vertices based on component type
    uint compType = compFn.componentType();
    if (compType == MFn::kMeshVertComponent)
    {
        outIndices = elems;
    }
    else if (compType == MFn::kMeshEdgeComponent)
    {
        for (uint i=0; i < elemLen; i++)
        {
            int2 edgeVerts;
            meshFn.getEdgeVertices(elems[i], edgeVerts);
            outIndices.append(edgeVerts[0]);
            outIndices.append(edgeVerts[1]);
        }
    }
    else if (compType == MFn::kMeshPolygonComponent)
    {
        for (uint i=0; i < elemLen; i++)
        {
            // Grab verts for the current poly
            MIntArray polyVerts;
            meshFn.getPolygonVertices(elems[i], polyVerts);
            uint polyVertsLen = polyVerts.length();
            for (uint j=0; j < polyVertsLen; j++)
            {
                outIndices.append(polyVerts[j]);
            }
        }
    }
    else if (compType == MFn::kMeshFaceVertComponent)
    {
        // I think this is how you convert face to object
        // relative vertices...
        MIntArray faceCounts;
        MIntArray faceVerts;
        meshFn.getVertices(faceCounts, faceVerts);
        for (uint j=0; j < elemLen; j++)
        {
            outIndices.append(faceVerts[j]);
        }
    }
    else
    {
        // Not supported
        return MStatus::kNotImplemented;
    }
    return MStatus::kSuccess;
}
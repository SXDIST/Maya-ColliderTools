//
// ColliderHullNode.cpp
// ColliderTools
//
// Created by Jonathan Tilden on 12/30/12.
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

#include "ColliderHullNode.h"
#include "ColliderFit.h"
#include "ColliderHull.h"
#include <maya/MArrayDataHandle.h>
#include <maya/MDataHandle.h>
#include <maya/MStatus.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnComponentListData.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnMesh.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MGlobal.h>

const MTypeId ColliderHullNode::id(0x0012b000);
const char* ColliderHullNode::typeName = "ctColliderHull";

MObject ColliderHullNode::inputAttr;
MObject ColliderHullNode::inputComponentsAttr;
MObject ColliderHullNode::inputPolymeshAttr;
MObject ColliderHullNode::outputPolymeshAttr;
MObject ColliderHullNode::useSkinWidthAttr;
MObject ColliderHullNode::skinWidthAttr;
MObject ColliderHullNode::normalEpsilonAttr;
MObject ColliderHullNode::useTrianglesAttr;
MObject ColliderHullNode::maxOutputVerticesAttr;
MObject ColliderHullNode::useReverseTriOrderAttr;


void* ColliderHullNode::creator()
{
    return new ColliderHullNode;
}


MStatus ColliderHullNode::initialize()
{
    MStatus status;

    MFnTypedAttribute inputPolymeshAttrFn;
    inputPolymeshAttr = inputPolymeshAttrFn.create("inputPolymesh", "ip", MFnData::kMesh, &status);
    if (!status)
    {
        return status;
    }
    inputPolymeshAttrFn.setDisconnectBehavior(MFnAttribute::kDelete);
    inputPolymeshAttrFn.setKeyable(false);

    MFnTypedAttribute inputComponentsAttrFn;
    inputComponentsAttr = inputComponentsAttrFn.create("inputComponents", "ics", MFnData::kComponentList, &status);
    if (!status)
    {
        return status;
    }
    inputComponentsAttrFn.setDisconnectBehavior(MFnAttribute::kReset);
    inputComponentsAttrFn.setKeyable(false);

    MFnCompoundAttribute inputAttrFn;
    inputAttr = inputAttrFn.create("input", "input", &status);
    if (!status)
    {
        return status;
    }
    inputAttrFn.addChild(inputPolymeshAttr);
    inputAttrFn.addChild(inputComponentsAttr);
    inputAttrFn.setArray(true);
    inputAttrFn.setIndexMatters(false);

    MFnTypedAttribute outputPolymeshAttrFn;
    outputPolymeshAttr = outputPolymeshAttrFn.create("output", "out", MFnData::kMesh, &status);
    if (!status)
    {
        return status;
    }
    outputPolymeshAttrFn.setWritable(false);
    outputPolymeshAttrFn.setStorable(false);
    outputPolymeshAttrFn.setKeyable(false);

    MFnNumericAttribute useSkinWidthAttrFn;
    useSkinWidthAttr = useSkinWidthAttrFn.create("skinWidthEnabled", "skwen", MFnNumericData::kBoolean, false, &status);
    if (!status)
    {
        return status;
    }
    useSkinWidthAttrFn.setWritable(true);
    useSkinWidthAttrFn.setStorable(true);
    useSkinWidthAttrFn.setKeyable(true);

    MFnNumericAttribute skinWidthFn;
    skinWidthAttr = skinWidthFn.create("skinWidth", "skw", MFnNumericData::kDouble, 0.01, &status);
    if (!status)
    {
        return status;
    }
    skinWidthFn.setWritable(true);
    skinWidthFn.setStorable(true);
    skinWidthFn.setKeyable(true);

    MFnNumericAttribute normalEpsilonAttrFn;
    normalEpsilonAttr = normalEpsilonAttrFn.create("normalEpsilon", "ep", MFnNumericData::kDouble, 0.001, &status);
    if (!status)
    {
        return status;
    }
    normalEpsilonAttrFn.setWritable(true);
    normalEpsilonAttrFn.setStorable(true);
    normalEpsilonAttrFn.setKeyable(true);
    normalEpsilonAttrFn.setMin(0.000001);

    MFnNumericAttribute useTrianglesAttrFn;
    useTrianglesAttr = useTrianglesAttrFn.create("forceTriangles", "tri", MFnNumericData::kBoolean, true, &status);
    if (!status)
    {
        return status;
    }
    useTrianglesAttrFn.setWritable(true);
    useTrianglesAttrFn.setStorable(true);
    useTrianglesAttrFn.setKeyable(true);
    useTrianglesAttrFn.setHidden(true);

    MFnNumericAttribute maxOutputVerticesAttrFn;
    maxOutputVerticesAttr = maxOutputVerticesAttrFn.create("maxVertices", "max", MFnNumericData::kInt, 4096, &status);
    if (!status)
    {
        return status;
    }
    maxOutputVerticesAttrFn.setWritable(true);
    maxOutputVerticesAttrFn.setStorable(true);
    maxOutputVerticesAttrFn.setKeyable(true);
    maxOutputVerticesAttrFn.setMin(4);
    maxOutputVerticesAttrFn.setMax(4096);

    MFnNumericAttribute useReverseTriOrderAttrFn;
    useReverseTriOrderAttr = useReverseTriOrderAttrFn.create("reverseNormals", "rev", MFnNumericData::kBoolean, false, &status);
    if (!status)
    {
        return status;
    }
    useReverseTriOrderAttrFn.setWritable(true);
    useReverseTriOrderAttrFn.setStorable(true);
    useReverseTriOrderAttrFn.setKeyable(true);

    addAttribute(useSkinWidthAttr);
    addAttribute(skinWidthAttr);
    addAttribute(normalEpsilonAttr);
    addAttribute(useTrianglesAttr);
    addAttribute(maxOutputVerticesAttr);
    addAttribute(useReverseTriOrderAttr);
    addAttribute(inputAttr);
    addAttribute(outputPolymeshAttr);

    attributeAffects(useSkinWidthAttr, outputPolymeshAttr);
    attributeAffects(skinWidthAttr, outputPolymeshAttr);
    attributeAffects(normalEpsilonAttr, outputPolymeshAttr);
    attributeAffects(useTrianglesAttr, outputPolymeshAttr);
    attributeAffects(maxOutputVerticesAttr, outputPolymeshAttr);
    attributeAffects(useReverseTriOrderAttr, outputPolymeshAttr);
    attributeAffects(inputAttr, outputPolymeshAttr);
    attributeAffects(inputPolymeshAttr, outputPolymeshAttr);
    attributeAffects(inputComponentsAttr, outputPolymeshAttr);

    return MStatus::kSuccess;
}

MStatus ColliderHullNode::processInputIndex(MPointArray& allPoints, MDataHandle& meshHndl, MDataHandle& compHndl)
{
    MStatus status;
    MObject mesh = meshHndl.asMeshTransformed();
    if (mesh.isNull())
    {
        return MStatus::kFailure;
    }

    MFnMesh meshFn(mesh, &status);
    if (!status)
    {
        return status;
    }

    MPointArray meshPoints;
    status = meshFn.getPoints(meshPoints);
    if (!status)
    {
        return status;
    }

    MObject components = compHndl.data();
    if (components.isNull())
    {
        for (unsigned int i = 0; i < meshPoints.length(); ++i)
        {
            allPoints.append(meshPoints[i]);
        }
        return MStatus::kSuccess;
    }

    MFnComponentListData componentListFn(components, &status);
    if (!status)
    {
        return status;
    }

    for (unsigned int i = 0; i < componentListFn.length(); ++i)
    {
        MObject component = componentListFn[i];
        if (!component.hasFn(MFn::kSingleIndexedComponent))
        {
            continue;
        }

        MFnSingleIndexedComponent componentFn(component, &status);
        if (!status)
        {
            return status;
        }

        if (componentFn.isComplete())
        {
            for (unsigned int j = 0; j < meshPoints.length(); ++j)
            {
                allPoints.append(meshPoints[j]);
            }
            continue;
        }

        MIntArray vertexIndices;
        status = ColliderTools::componentToVertexIds(vertexIndices, mesh, component);
        if (status == MStatus::kNotImplemented)
        {
            continue;
        }
        if (!status)
        {
            return status;
        }

        for (unsigned int j = 0; j < vertexIndices.length(); ++j)
        {
            MPoint point;
            status = meshFn.getPoint(vertexIndices[j], point);
            if (!status)
            {
                return status;
            }
            allPoints.append(point);
        }
    }

    return MStatus::kSuccess;
}

MStatus ColliderHullNode::compute(const MPlug& plug, MDataBlock& data)
{
    if (plug != outputPolymeshAttr)
    {
        return MStatus::kUnknownParameter;
    }

    MStatus status;
    MFnMeshData outputDataCreator;
    MObject outputMesh = outputDataCreator.create(&status);
    if (!status)
    {
        return status;
    }

    MDataHandle outputData = data.outputValue(outputPolymeshAttr, &status);
    if (!status)
    {
        return status;
    }

    MPointArray allPoints;
    MArrayDataHandle inputData(data.inputArrayValue(inputAttr, &status));
    if (!status)
    {
        return status;
    }

    const unsigned int elementCount = inputData.elementCount();
    if (elementCount == 0)
    {
        return MStatus::kInvalidParameter;
    }

    for (unsigned int i = 0; i < elementCount; ++i)
    {
        MDataHandle element = inputData.inputValue(&status);
        if (!status)
        {
            return status;
        }

        MDataHandle meshHndl = element.child(inputPolymeshAttr);
        MDataHandle compHndl = element.child(inputComponentsAttr);
        status = processInputIndex(allPoints, meshHndl, compHndl);
        if (!status)
        {
            return status;
        }
        inputData.next();
    }

    allPoints = ColliderTools::cleanPointSet(allPoints);
    if (allPoints.length() < 4)
    {
        MGlobal::displayError("At least 4 unique points are required to compute the hull.");
        return MStatus::kFailure;
    }

    ColliderTools::HullOptions hullOptions;

    hullOptions.useSkinWidth = data.inputValue(useSkinWidthAttr, &status).asBool();
    if (!status)
    {
        return status;
    }

    hullOptions.skinWidth = data.inputValue(skinWidthAttr, &status).asDouble();
    if (!status)
    {
        return status;
    }

    hullOptions.normalEpsilon = data.inputValue(normalEpsilonAttr, &status).asDouble();
    if (!status)
    {
        return status;
    }

    hullOptions.forceTriangles = data.inputValue(useTrianglesAttr, &status).asBool();
    if (!status)
    {
        return status;
    }

    hullOptions.maxOutputVertices = data.inputValue(maxOutputVerticesAttr, &status).asInt();
    if (!status)
    {
        return status;
    }

    hullOptions.reverseTriangleOrder = data.inputValue(useReverseTriOrderAttr, &status).asBool();
    if (!status)
    {
        return status;
    }

    status = ColliderTools::createHullMeshData(outputMesh, allPoints, hullOptions);
    if (!status)
    {
        MGlobal::displayError("Convex hull generation failed. Select non-degenerate, non-coplanar mesh points.");
        return status;
    }

    outputData.set(outputMesh);
    data.setClean(outputPolymeshAttr);
    return MStatus::kSuccess;
}
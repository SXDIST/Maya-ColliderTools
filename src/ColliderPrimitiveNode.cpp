#include "ColliderPrimitiveNode.h"

#include "ColliderFit.h"
#include "ColliderGeometry.h"

#include "ColliderHull.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnComponentListData.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPxManipContainer.h>

const MTypeId ColliderPrimitiveNode::id(0x0012b001);
const char* ColliderPrimitiveNode::typeName = "ctColliderPrimitive";

MObject ColliderPrimitiveNode::inputMeshAttr;
MObject ColliderPrimitiveNode::inputComponentsAttr;
MObject ColliderPrimitiveNode::colliderTypeAttr;
MObject ColliderPrimitiveNode::segmentsAttr;
MObject ColliderPrimitiveNode::radiusScaleAttr;
MObject ColliderPrimitiveNode::lengthScaleAttr;
MObject ColliderPrimitiveNode::outputMeshAttr;

void* ColliderPrimitiveNode::creator()
{
    return new ColliderPrimitiveNode;
}

MStatus ColliderPrimitiveNode::initialize()
{
    MStatus status;

    MFnTypedAttribute typedAttr;
    inputMeshAttr = typedAttr.create("inputMesh", "in", MFnData::kMesh, MObject::kNullObj, &status);
    if (!status)
    {
        return status;
    }
    typedAttr.setReadable(false);
    typedAttr.setWritable(true);
    typedAttr.setStorable(false);

    inputComponentsAttr = typedAttr.create("inputComponents", "ics", MFnData::kComponentList, MObject::kNullObj, &status);
    if (!status)
    {
        return status;
    }
    typedAttr.setReadable(false);
    typedAttr.setWritable(true);
    typedAttr.setStorable(false);

    MFnEnumAttribute enumAttr;
    colliderTypeAttr = enumAttr.create("colliderType", "typ", 0, &status);
    if (!status)
    {
        return status;
    }
    enumAttr.addField("Cylinder", 0);
    enumAttr.addField("Capsule", 1);
    enumAttr.setWritable(true);
    enumAttr.setStorable(true);
    enumAttr.setKeyable(true);

    MFnNumericAttribute numericAttr;
    segmentsAttr = numericAttr.create("segments", "seg", MFnNumericData::kInt, 16, &status);
    if (!status)
    {
        return status;
    }
    numericAttr.setMin(6);
    numericAttr.setWritable(true);
    numericAttr.setStorable(true);
    numericAttr.setKeyable(true);

    radiusScaleAttr = numericAttr.create("radiusScale", "rsc", MFnNumericData::kDouble, 1.0, &status);
    if (!status)
    {
        return status;
    }
    numericAttr.setMin(0.001);
    numericAttr.setWritable(true);
    numericAttr.setStorable(true);
    numericAttr.setKeyable(true);

    lengthScaleAttr = numericAttr.create("lengthScale", "lsc", MFnNumericData::kDouble, 1.0, &status);
    if (!status)
    {
        return status;
    }
    numericAttr.setMin(0.001);
    numericAttr.setWritable(true);
    numericAttr.setStorable(true);
    numericAttr.setKeyable(true);

    outputMeshAttr = typedAttr.create("outputMesh", "out", MFnData::kMesh, MObject::kNullObj, &status);
    if (!status)
    {
        return status;
    }
    typedAttr.setReadable(true);
    typedAttr.setWritable(false);
    typedAttr.setStorable(false);

    addAttribute(inputMeshAttr);
    addAttribute(inputComponentsAttr);
    addAttribute(colliderTypeAttr);
    addAttribute(segmentsAttr);
    addAttribute(radiusScaleAttr);
    addAttribute(lengthScaleAttr);
    addAttribute(outputMeshAttr);

    attributeAffects(inputMeshAttr, outputMeshAttr);
    attributeAffects(inputComponentsAttr, outputMeshAttr);
    attributeAffects(colliderTypeAttr, outputMeshAttr);
    attributeAffects(segmentsAttr, outputMeshAttr);
    attributeAffects(radiusScaleAttr, outputMeshAttr);
    attributeAffects(lengthScaleAttr, outputMeshAttr);

    MTypeId nodeId(id);
    status = MPxManipContainer::addToManipConnectTable(nodeId);
    if (!status)
    {
        return status;
    }

    return MStatus::kSuccess;
}

MStatus ColliderPrimitiveNode::compute(const MPlug& plug, MDataBlock& data)
{
    if (plug != outputMeshAttr)
    {
        return MStatus::kUnknownParameter;
    }

    MStatus status;
    MObject inputMesh = data.inputValue(inputMeshAttr, &status).asMesh();
    if (!status || inputMesh.isNull())
    {
        return MStatus::kInvalidParameter;
    }

    MFnMesh inputMeshFn(inputMesh, &status);
    if (!status)
    {
        return status;
    }

    MPointArray meshPoints;
    status = inputMeshFn.getPoints(meshPoints, MSpace::kObject);
    if (!status || meshPoints.length() == 0)
    {
        return MStatus::kInvalidParameter;
    }

    MPointArray points;
    MObject components = data.inputValue(inputComponentsAttr, &status).data();
    if (status && !components.isNull())
    {
        MFnComponentListData componentListFn(components, &status);
        if (status)
        {
            for (unsigned int i = 0; i < componentListFn.length(); ++i)
            {
                MObject component = componentListFn[i];
                if (!component.hasFn(MFn::kSingleIndexedComponent))
                {
                    continue;
                }

                MIntArray vertexIds;
                status = ColliderTools::componentToVertexIds(vertexIds, inputMesh, component);
                if (!status)
                {
                    return status;
                }
                for (unsigned int j = 0; j < vertexIds.length(); ++j)
                {
                    const int index = vertexIds[j];
                    if (index >= 0 && static_cast<unsigned int>(index) < meshPoints.length())
                    {
                        points.append(meshPoints[index]);
                    }
                }
            }
        }
    }
    if (points.length() == 0)
    {
        points = meshPoints;
    }
    points = ColliderTools::cleanPointSet(points);
    if (points.length() == 0)
    {
        return MStatus::kInvalidParameter;
    }

    int colliderType = data.inputValue(colliderTypeAttr, &status).asShort();
    if (!status)
    {
        return status;
    }
    int segments = data.inputValue(segmentsAttr, &status).asInt();
    if (!status)
    {
        return status;
    }
    const double radiusScale = data.inputValue(radiusScaleAttr, &status).asDouble();
    if (!status)
    {
        return status;
    }
    const double lengthScale = data.inputValue(lengthScaleAttr, &status).asDouble();
    if (!status)
    {
        return status;
    }

    const ColliderTools::BoxFit fit = ColliderTools::fitObb(points);
    MVector center;
    MVector axis;
    double halfLength = 0.0;
    double radius = 0.0;
    ColliderTools::fitAxisShape(points, fit.axes, center, axis, halfLength, radius);
    halfLength *= lengthScale;
    radius *= radiusScale;

    ColliderTools::MeshData mesh = colliderType == 0
        ? ColliderTools::makeCylinderMesh(center, axis, halfLength, radius, segments)
        : ColliderTools::makeCapsuleMesh(center, axis, halfLength, radius, segments);
    ColliderTools::orientFacesOutward(mesh);

    MFnMeshData meshDataFn;
    MObject outputDataObject = meshDataFn.create(&status);
    if (!status)
    {
        return status;
    }

    MFnMesh outputMeshFn;
    MObject outputMesh = outputMeshFn.create(mesh.points.length(), mesh.faceCounts.length(), mesh.points, mesh.faceCounts, mesh.faceConnects, outputDataObject, &status);
    if (!status)
    {
        return status;
    }
    ColliderTools::hardenMeshEdges(outputMesh);

    MDataHandle outputHandle = data.outputValue(outputMeshAttr, &status);
    if (!status)
    {
        return status;
    }
    outputHandle.set(outputDataObject);
    data.setClean(outputMeshAttr);
    return MStatus::kSuccess;
}

#include "ColliderCreateCmd.h"

#include "ColliderFit.h"
#include "ColliderGeometry.h"
#include "ColliderPrimitiveNode.h"
#include "DDConvexHullNode.h"
#include "DDConvexHullUtils.h"

#include <maya/MArgDatabase.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnTransform.h>
#include <maya/MFnComponentListData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MDGModifier.h>
#include <maya/MPlug.h>
#include <maya/MItDag.h>
#include <maya/MGlobal.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MDagModifier.h>
#include <maya/MObjectArray.h>
#include <maya/MStringArray.h>
#include <maya/MTransformationMatrix.h>
#include <algorithm>
#include <cctype>
#include <vector>

const char* ColliderCreateCmd::commandName = "ctCreateCollider";

namespace
{
const char* kTypeFlag = "-t";
const char* kTypeLongFlag = "-type";
const char* kNameFlag = "-n";
const char* kNameLongFlag = "-name";
const char* kSegmentsFlag = "-s";
const char* kSegmentsLongFlag = "-segments";
const char* kMaxVerticesFlag = "-mv";
const char* kMaxVerticesLongFlag = "-maxVertices";
const char* kSkinWidthFlag = "-sw";
const char* kSkinWidthLongFlag = "-skinWidth";
const char* kUseSkinWidthFlag = "-usw";
const char* kUseSkinWidthLongFlag = "-useSkinWidth";
const char* kCombineFlag = "-c";
const char* kCombineLongFlag = "-combine";
const char* kUnrealFlag = "-u";
const char* kUnrealLongFlag = "-unreal";
const char* kHistoryFlag = "-hi";
const char* kHistoryLongFlag = "-history";

MString lower(const MString& value)
{
    MString result(value);
    result.toLowerCase();
    return result;
}

MString defaultName(const MString& type, int index, bool multiple)
{
    MString name = "ct_";
    name += type;
    name += "Collider";
    if (multiple)
    {
        name += index + 1;
    }
    name += "#";
    return name;
}

MString sourceName(const MDagPath& path)
{
    MDagPath sourcePath(path);
    if (sourcePath.apiType() == MFn::kMesh && sourcePath.length() > 0)
    {
        sourcePath.pop();
    }

    MString name = sourcePath.partialPathName();
    const int pipeIndex = name.rindex('|');
    if (pipeIndex >= 0)
    {
        name = name.substring(pipeIndex + 1, name.length() - 1);
    }
    const int namespaceIndex = name.rindex(':');
    if (namespaceIndex >= 0)
    {
        name = name.substring(namespaceIndex + 1, name.length() - 1);
    }
    return name;
}

MString unrealPrefix(const MString& type)
{
    if (type == "box" || type == "obox")
    {
        return "UBX_";
    }
    if (type == "capsule")
    {
        return "UCP_";
    }
    return "UCX_";
}

MString unrealName(const MString& type, const MString& source, int index)
{
    MString name = unrealPrefix(type);
    name += source.length() > 0 ? source : "Collider";
    name += "_";
    if (index + 1 < 10)
    {
        name += "0";
    }
    name += index + 1;
    return name;
}

MString colliderGroupName(const MString& type)
{
    if (type == "box" || type == "obox")
    {
        return "UBX";
    }
    if (type == "capsule")
    {
        return "UCP";
    }
    return "UCX";
}

MStatus findTransformByName(const MString& name, MObject& transform)
{
    MSelectionList selection;
    MStatus status = selection.add(name);
    if (!status || selection.length() == 0)
    {
        return MStatus::kNotFound;
    }
    status = selection.getDependNode(0, transform);
    if (!status || !transform.hasFn(MFn::kTransform))
    {
        return MStatus::kNotFound;
    }
    return MStatus::kSuccess;
}

MStatus findOrCreateTransform(const MString& name, const MObject& parent, MObject& transform)
{
    if (findTransformByName(name, transform) == MStatus::kSuccess)
    {
        return MStatus::kSuccess;
    }

    MStatus status;
    MFnTransform transformFn;
    transform = transformFn.create(parent, &status);
    if (!status)
    {
        return status;
    }
    transformFn.setName(name, false, &status);
    return status;
}

MStatus parentCollider(MObject& transform, const MString& type)
{
    MStatus status;
    MObject rootGroup;
    status = findOrCreateTransform("Colliders", MObject::kNullObj, rootGroup);
    if (!status)
    {
        return status;
    }

    MObject typeGroup;
    status = findOrCreateTransform(colliderGroupName(type), rootGroup, typeGroup);
    if (!status)
    {
        return status;
    }

    MFnDagNode transformDag(transform, &status);
    if (!status)
    {
        return status;
    }

    MObject currentParent = transformDag.parent(0, &status);
    if (status && currentParent == typeGroup)
    {
        return MStatus::kSuccess;
    }

    MDagModifier dagModifier;
    status = dagModifier.reparentNode(transform, typeGroup);
    if (!status)
    {
        return status;
    }
    return dagModifier.doIt();
}

MStatus conformMeshNormals(const MObject& shape)
{
    MStatus status;
    MFnMesh meshFn(shape, &status);
    if (!status)
    {
        return status;
    }

    MPointArray points;
    status = meshFn.getPoints(points, MSpace::kWorld);
    if (!status || points.length() == 0)
    {
        return status;
    }

    MVector center;
    for (unsigned int i = 0; i < points.length(); ++i)
    {
        center += MVector(points[i]);
    }
    center /= static_cast<double>(points.length());

    MIntArray reverseFaces;
    MItMeshPolygon polygonIt(shape, &status);
    if (!status)
    {
        return status;
    }

    for (; !polygonIt.isDone(); polygonIt.next())
    {
        MPoint faceCenter = polygonIt.center(MSpace::kWorld, &status);
        if (!status)
        {
            continue;
        }
        MVector normal;
        status = polygonIt.getNormal(normal, MSpace::kWorld);
        if (status && normal.length() > 1.0e-8 && normal * (MVector(faceCenter) - center) < 0.0)
        {
            reverseFaces.append(polygonIt.index());
        }
    }

    return MStatus::kSuccess;
}

MStatus createHull(const MPointArray& points, const DDConvexHullUtils::hullOpts& options, const MString& name, MObject& transform)
{
    const MPointArray cleanPoints = ColliderTools::cleanPointSet(points);
    if (cleanPoints.length() < 4)
    {
        return MStatus::kInvalidParameter;
    }

    MStatus status;
    MFnTransform transformFn;
    transform = transformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return status;
    }
    transformFn.setName(name, false, &status);
    if (!status)
    {
        return status;
    }

    MFnMeshData meshDataFn;
    MObject meshData = meshDataFn.create(&status);
    if (!status)
    {
        return status;
    }

    status = DDConvexHullUtils::generateMayaHull(meshData, cleanPoints, options);
    if (!status && options.maxOutputVertices < 4096)
    {
        DDConvexHullUtils::hullOpts fallbackOptions = options;
        fallbackOptions.maxOutputVertices = 4096;
        status = DDConvexHullUtils::generateMayaHull(meshData, cleanPoints, fallbackOptions);
    }
    if (!status)
    {
        MGlobal::displayError("Convex hull generation failed. Select a polygon mesh or non-coplanar mesh components.");
        return status;
    }

    MFnMesh meshFn(meshData, &status);
    if (!status)
    {
        return status;
    }

    MObject shape = meshFn.copy(meshData, transform, &status);
    if (!status)
    {
        MGlobal::displayError("Convex hull mesh copy failed.");
        return status;
    }

    MFnDagNode shapeFn(shape, &status);
    if (status)
    {
        shapeFn.setName(MString(transformFn.name()) + "Shape", false, &status);
    }

    conformMeshNormals(shape);
    ColliderTools::hardenMeshEdges(shape);
    ColliderTools::assignInitialShadingGroup(shape);
    return MStatus::kSuccess;
}

MStatus createHistoryMeshTransform(const MString& name, MObject& transform, MObject& shape)
{
    MStatus status;
    MFnTransform transformFn;
    transform = transformFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        return status;
    }
    transformFn.setName(name, false, &status);
    if (!status)
    {
        return status;
    }

    MFnMesh meshFn;
    MPointArray points;
    points.append(MPoint(-0.5, 0.0, -0.5));
    points.append(MPoint(0.5, 0.0, -0.5));
    points.append(MPoint(0.5, 0.0, 0.5));
    points.append(MPoint(-0.5, 0.0, 0.5));
    MIntArray faceCounts;
    faceCounts.append(4);
    MIntArray faceConnects;
    faceConnects.append(0);
    faceConnects.append(1);
    faceConnects.append(2);
    faceConnects.append(3);
    shape = meshFn.create(points.length(), faceCounts.length(), points, faceCounts, faceConnects, transform, &status);
    if (!status)
    {
        return status;
    }

    MFnDagNode shapeFn(shape, &status);
    if (status)
    {
        shapeFn.setName(MString(transformFn.name()) + "Shape", false, &status);
    }
    ColliderTools::assignInitialShadingGroup(shape);
    return MStatus::kSuccess;
}

MStatus sourceMeshPath(const MDagPath& sourcePath, MDagPath& meshPath)
{
    meshPath = sourcePath;
    if (meshPath.apiType() != MFn::kMesh)
    {
        MStatus status = meshPath.extendToShape();
        if (!status)
        {
            return status;
        }
    }
    return meshPath.hasFn(MFn::kMesh) ? MStatus::kSuccess : MStatus::kInvalidParameter;
}

MStatus matchSourceTransform(const MDagPath& sourcePath, MObject& transform)
{
    MDagPath transformPath(sourcePath);
    if (transformPath.apiType() == MFn::kMesh && transformPath.length() > 0)
    {
        transformPath.pop();
    }
    if (!transformPath.hasFn(MFn::kTransform))
    {
        return MStatus::kSuccess;
    }

    MStatus status;
    MFnTransform sourceTransformFn(transformPath, &status);
    if (!status)
    {
        return status;
    }
    MFnTransform outputTransformFn(transform, &status);
    if (!status)
    {
        return status;
    }
    return outputTransformFn.set(sourceTransformFn.transformation(&status));
}

MStatus setComponentPlug(const MPlug& plug, const MObject& component)
{
    if (component.isNull())
    {
        return MStatus::kSuccess;
    }

    MStatus status;
    MFnComponentListData componentListFn;
    MObject componentList = componentListFn.create(&status);
    if (!status)
    {
        return status;
    }
    MObject componentCopy(component);
    status = componentListFn.add(componentCopy);
    if (!status)
    {
        return status;
    }
    MPlug writablePlug(plug);
    return writablePlug.setMObject(componentList);
}

MStatus connectHistoryNode(const MDagPath& sourcePath, const MObject& component, const MObject& historyNode, const MObject& outputShape)
{
    MStatus status;
    MDagPath meshPath;
    status = sourceMeshPath(sourcePath, meshPath);
    if (!status)
    {
        return status;
    }

    MFnDagNode sourceMeshFn(meshPath, &status);
    if (!status)
    {
        return status;
    }
    MFnDependencyNode historyFn(historyNode, &status);
    if (!status)
    {
        return status;
    }
    MFnDependencyNode outputShapeFn(outputShape, &status);
    if (!status)
    {
        return status;
    }

    MPlug sourceMesh = sourceMeshFn.findPlug("outMesh", true, &status);
    if (!status)
    {
        return status;
    }
    MPlug outputInMesh = outputShapeFn.findPlug("inMesh", true, &status);
    if (!status)
    {
        return status;
    }

    MDGModifier dgModifier;
    MPlug historyInput;
    MPlug historyOutput;
    if (historyNode.hasFn(MFn::kPluginDependNode))
    {
        if (historyFn.typeName() == ColliderPrimitiveNode::typeName)
        {
            historyInput = historyFn.findPlug("inputMesh", true, &status);
            if (!status)
            {
                return status;
            }
            historyOutput = historyFn.findPlug("outputMesh", true, &status);
            if (!status)
            {
                return status;
            }
            MPlug inputComponents = historyFn.findPlug("inputComponents", true, &status);
            if (!status)
            {
                return status;
            }
            status = setComponentPlug(inputComponents, component);
            if (!status)
            {
                return status;
            }
            dgModifier.connect(sourceMesh, historyInput);
        }
        else
        {
            MPlug inputArray = historyFn.findPlug("input", true, &status);
            if (!status)
            {
                return status;
            }
            MPlug inputElement = inputArray.elementByLogicalIndex(0, &status);
            if (!status)
            {
                return status;
            }
            MPlug inputMesh = inputElement.child(DDConvexHullNode::inputPolymeshAttr, &status);
            if (!status)
            {
                return status;
            }
            MPlug inputComponents = inputElement.child(DDConvexHullNode::inputComponentsAttr, &status);
            if (!status)
            {
                return status;
            }
            status = setComponentPlug(inputComponents, component);
            if (!status)
            {
                return status;
            }
            historyOutput = historyFn.findPlug("output", true, &status);
            if (!status)
            {
                return status;
            }
            dgModifier.connect(sourceMesh, inputMesh);
        }
    }

    dgModifier.connect(historyOutput, outputInMesh);
    return dgModifier.doIt();
}

MStatus createHistoryCollider(const MString& type, const MDagPath& sourcePath, const MObject& component, int segments, const DDConvexHullUtils::hullOpts& hullOptions, const MString& name, MObject& transform, MObject& historyNode)
{
    MStatus status;
    MObject shape;
    status = createHistoryMeshTransform(name, transform, shape);
    if (!status)
    {
        return status;
    }

    status = parentCollider(transform, type);
    if (!status)
    {
        return status;
    }

    status = matchSourceTransform(sourcePath, transform);
    if (!status)
    {
        return status;
    }

    MFnDependencyNode historyFn;
    if (type == "hull")
    {
        historyNode = historyFn.create("DDConvexHull", MString(name) + "History", &status);
        if (!status)
        {
            return status;
        }
        historyFn.findPlug("maxVertices", true, &status).setInt(static_cast<int>(hullOptions.maxOutputVertices));
    }
    else
    {
        historyNode = historyFn.create(ColliderPrimitiveNode::typeName, MString(name) + "History", &status);
        if (!status)
        {
            return status;
        }
        historyFn.findPlug("colliderType", true, &status).setShort(type == "capsule" ? 1 : 0);
        historyFn.findPlug("segments", true, &status).setInt(segments);
    }

    return connectHistoryNode(sourcePath, component, historyNode, shape);
}

MStatus createPrimitive(const MString& type, const MPointArray& points, int segments, const MString& name, MObject& transform)
{
    const MPointArray cleanPoints = ColliderTools::cleanPointSet(points);
    if (cleanPoints.length() == 0)
    {
        return MStatus::kInvalidParameter;
    }

    ColliderTools::MeshData mesh;
    if (type == "box")
    {
        const ColliderTools::BoxFit fit = ColliderTools::fitAabb(cleanPoints);
        mesh = ColliderTools::makeBoxMesh(fit.center, fit.axes, fit.halfExtents);
    }
    else if (type == "obox")
    {
        const ColliderTools::BoxFit fit = ColliderTools::fitObb(cleanPoints);
        mesh = ColliderTools::makeBoxMesh(fit.center, fit.axes, fit.halfExtents);
    }
    else if (type == "cylinder" || type == "capsule")
    {
        const ColliderTools::BoxFit fit = ColliderTools::fitObb(cleanPoints);
        MVector center;
        MVector axis;
        double halfLength = 0.0;
        double radius = 0.0;
        ColliderTools::fitAxisShape(cleanPoints, fit.axes, center, axis, halfLength, radius);
        if (type == "cylinder")
        {
            mesh = ColliderTools::makeCylinderMesh(center, axis, halfLength, radius, segments);
        }
        else
        {
            mesh = ColliderTools::makeCapsuleMesh(center, axis, halfLength, radius, segments);
        }
    }
    else
    {
        MGlobal::displayError("Unsupported collider type: " + type);
        return MStatus::kInvalidParameter;
    }

    MObject shape;
    return ColliderTools::createMeshTransform(mesh, name, transform, shape);
}
}

void* ColliderCreateCmd::creator()
{
    return new ColliderCreateCmd;
}

MSyntax ColliderCreateCmd::syntax()
{
    MSyntax syntax;
    syntax.addFlag(kTypeFlag, kTypeLongFlag, MSyntax::kString);
    syntax.addFlag(kNameFlag, kNameLongFlag, MSyntax::kString);
    syntax.addFlag(kSegmentsFlag, kSegmentsLongFlag, MSyntax::kLong);
    syntax.addFlag(kMaxVerticesFlag, kMaxVerticesLongFlag, MSyntax::kLong);
    syntax.addFlag(kSkinWidthFlag, kSkinWidthLongFlag, MSyntax::kDouble);
    syntax.addFlag(kUseSkinWidthFlag, kUseSkinWidthLongFlag, MSyntax::kBoolean);
    syntax.addFlag(kCombineFlag, kCombineLongFlag, MSyntax::kBoolean);
    syntax.addFlag(kUnrealFlag, kUnrealLongFlag, MSyntax::kBoolean);
    syntax.addFlag(kHistoryFlag, kHistoryLongFlag, MSyntax::kBoolean);
    syntax.enableQuery(false);
    syntax.enableEdit(false);
    return syntax;
}

MStatus ColliderCreateCmd::doIt(const MArgList& args)
{
    MStatus status;
    MArgDatabase argData(syntax(), args, &status);
    if (!status)
    {
        return status;
    }

    MString type = "box";
    if (argData.isFlagSet(kTypeFlag))
    {
        argData.getFlagArgument(kTypeFlag, 0, type);
    }
    type = lower(type);

    MString name;
    if (argData.isFlagSet(kNameFlag))
    {
        argData.getFlagArgument(kNameFlag, 0, name);
    }

    int segments = 16;
    if (argData.isFlagSet(kSegmentsFlag))
    {
        argData.getFlagArgument(kSegmentsFlag, 0, segments);
        segments = std::max(segments, 6);
    }

    bool combine = false;
    if (argData.isFlagSet(kCombineFlag))
    {
        argData.getFlagArgument(kCombineFlag, 0, combine);
    }

    bool unreal = false;
    if (argData.isFlagSet(kUnrealFlag))
    {
        argData.getFlagArgument(kUnrealFlag, 0, unreal);
    }

    bool history = false;
    if (argData.isFlagSet(kHistoryFlag))
    {
        argData.getFlagArgument(kHistoryFlag, 0, history);
    }

    DDConvexHullUtils::hullOpts hullOptions;
    hullOptions.forceTriangles = true;
    if (argData.isFlagSet(kMaxVerticesFlag))
    {
        int maxVertices = static_cast<int>(hullOptions.maxOutputVertices);
        argData.getFlagArgument(kMaxVerticesFlag, 0, maxVertices);
        hullOptions.maxOutputVertices = std::max(maxVertices, 4);
    }
    if (argData.isFlagSet(kSkinWidthFlag))
    {
        argData.getFlagArgument(kSkinWidthFlag, 0, hullOptions.skinWidth);
    }
    if (argData.isFlagSet(kUseSkinWidthFlag))
    {
        argData.getFlagArgument(kUseSkinWidthFlag, 0, hullOptions.useSkinWidth);
    }

    MSelectionList selection;
    MGlobal::getActiveSelectionList(selection);
    if (selection.length() == 0)
    {
        MGlobal::displayError("Select at least one mesh or mesh component.");
        return MStatus::kInvalidParameter;
    }

    std::vector<MObject> createdTransforms;
    std::vector<MObject> createdHistoryNodes;
    MStringArray createdNames;
    if (history && (type == "hull" || type == "cylinder" || type == "capsule"))
    {
        std::vector<ColliderTools::PointSet> pointSets;
        status = ColliderTools::gatherSelectionPointSets(selection, pointSets);
        if (!status || pointSets.empty())
        {
            MGlobal::displayError("No mesh found for procedural collider history.");
            return MStatus::kInvalidParameter;
        }

        for (size_t i = 0; i < pointSets.size(); ++i)
        {
            MObject transform;
            MObject historyNode;
            const MString outputName = name.length() > 0 && pointSets.size() == 1 ? name : (unreal ? unrealName(type, sourceName(pointSets[i].sourcePath), static_cast<int>(i)) : defaultName(type, static_cast<int>(i), pointSets.size() > 1));
            status = createHistoryCollider(type, pointSets[i].sourcePath, pointSets[i].component, segments, hullOptions, outputName, transform, historyNode);
            if (!status)
            {
                return status;
            }
            createdTransforms.push_back(transform);
            createdHistoryNodes.push_back(historyNode);
            MFnDagNode transformFn(transform);
            createdNames.append(transformFn.fullPathName());
        }
    }
    else if (combine)
    {
        MPointArray points;
        status = ColliderTools::gatherCombinedSelectionPoints(selection, points);
        if (!status || points.length() == 0)
        {
            MGlobal::displayError("No mesh points found in the current selection.");
            return MStatus::kInvalidParameter;
        }

        MObject transform;
        const MString outputName = name.length() > 0 ? name : (unreal ? unrealName(type, "Combined", 0) : defaultName(type, 0, false));
        status = type == "hull" ? createHull(points, hullOptions, outputName, transform) : createPrimitive(type, points, segments, outputName, transform);
        if (!status)
        {
            return status;
        }
        status = parentCollider(transform, type);
        if (!status)
        {
            return status;
        }
        MFnDagNode transformFn(transform, &status);
        if (!status)
        {
            return status;
        }
        for (unsigned int i = 0; i < transformFn.childCount(); ++i)
        {
            MObject child = transformFn.child(i, &status);
            if (status && child.hasFn(MFn::kMesh))
            {
                ColliderTools::assignInitialShadingGroup(child);
            }
        }
        createdTransforms.push_back(transform);
        createdNames.append(transformFn.fullPathName());
    }
    else
    {
        std::vector<ColliderTools::PointSet> pointSets;
        status = ColliderTools::gatherSelectionPointSets(selection, pointSets);
        if (!status || pointSets.empty())
        {
            MGlobal::displayError("No mesh points found in the current selection.");
            return MStatus::kInvalidParameter;
        }

        for (size_t i = 0; i < pointSets.size(); ++i)
        {
            MObject transform;
            const MString outputName = name.length() > 0 && pointSets.size() == 1 ? name : (unreal ? unrealName(type, sourceName(pointSets[i].sourcePath), static_cast<int>(i)) : defaultName(type, static_cast<int>(i), pointSets.size() > 1));
            status = type == "hull" ? createHull(pointSets[i].points, hullOptions, outputName, transform) : createPrimitive(type, pointSets[i].points, segments, outputName, transform);
            if (!status)
            {
                return status;
            }
            status = parentCollider(transform, type);
            if (!status)
            {
                return status;
            }
            MFnDagNode transformFn(transform, &status);
            if (!status)
            {
                return status;
            }
            for (unsigned int childIndex = 0; childIndex < transformFn.childCount(); ++childIndex)
            {
                MObject child = transformFn.child(childIndex, &status);
                if (status && child.hasFn(MFn::kMesh))
                {
                    ColliderTools::assignInitialShadingGroup(child);
                }
            }
            createdTransforms.push_back(transform);
            createdNames.append(transformFn.fullPathName());
        }
    }

    MSelectionList newSelection;
    for (MObject transform : createdTransforms)
    {
        newSelection.add(transform);
    }
    for (MObject historyNode : createdHistoryNodes)
    {
        newSelection.add(historyNode);
    }
    MGlobal::setActiveSelectionList(newSelection, MGlobal::kReplaceList);
    setResult(createdNames);
    return MStatus::kSuccess;
}

bool ColliderCreateCmd::isUndoable() const
{
    return false;
}

#include <maya/MFnPlugin.h>
#include <maya/MPxManipContainer.h>
#include "ColliderHullNode.h"
#include "ColliderCreateCmd.h"
#include "ColliderPrimitiveManip.h"
#include "ColliderPrimitiveNode.h"

MStatus initializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj, "ColliderTools", "0.1.0", "Any");
    status = plugin.registerNode(ColliderHullNode::typeName,
                                 ColliderHullNode::id,
                                 ColliderHullNode::creator,
                                 ColliderHullNode::initialize);
    if (!status)
    {
        status.perror("registerNode ctColliderHull");
        return status;
    }


    status = plugin.registerNode(ColliderPrimitiveNode::typeName,
                                 ColliderPrimitiveNode::id,
                                 ColliderPrimitiveNode::creator,
                                 ColliderPrimitiveNode::initialize);
    if (!status)
    {
        status.perror("registerNode ctColliderPrimitive");
        plugin.deregisterNode(ColliderHullNode::id);
        return status;
    }

    status = plugin.registerNode(ColliderPrimitiveManip::typeName,
                                 ColliderPrimitiveManip::id,
                                 ColliderPrimitiveManip::creator,
                                 ColliderPrimitiveManip::initialize,
                                 MPxNode::kManipContainer);
    if (!status)
    {
        status.perror("registerNode ctColliderPrimitiveManip");
        MTypeId primitiveNodeId(ColliderPrimitiveNode::id);
        MPxManipContainer::removeFromManipConnectTable(primitiveNodeId);
        plugin.deregisterNode(ColliderPrimitiveNode::id);
        plugin.deregisterNode(ColliderHullNode::id);
        return status;
    }

    status = plugin.registerCommand(ColliderCreateCmd::commandName,
                                    ColliderCreateCmd::creator,
                                    ColliderCreateCmd::syntax);
    if (!status)
    {
        status.perror("registerCommand ctCreateCollider");
        MTypeId primitiveNodeId(ColliderPrimitiveNode::id);
        MPxManipContainer::removeFromManipConnectTable(primitiveNodeId);
        plugin.deregisterNode(ColliderPrimitiveManip::id);
        plugin.deregisterNode(ColliderPrimitiveNode::id);
        plugin.deregisterNode(ColliderHullNode::id);
        return status;
    }
    return MS::kSuccess;
}

MStatus uninitializePlugin( MObject obj )
{
    MStatus status;
    MFnPlugin plugin( obj );
    status = plugin.deregisterCommand(ColliderCreateCmd::commandName);
    if (!status)
    {
        status.perror("deregisterCommand ctCreateCollider");
    }

    MTypeId primitiveNodeId(ColliderPrimitiveNode::id);
    status = MPxManipContainer::removeFromManipConnectTable(primitiveNodeId);
    if (!status)
    {
        status.perror("removeFromManipConnectTable ctColliderPrimitive");
    }

    status = plugin.deregisterNode(ColliderPrimitiveManip::id);
    if (!status)
    {
        status.perror("deregisterNode ctColliderPrimitiveManip");
    }

    status = plugin.deregisterNode(ColliderPrimitiveNode::id);
    if (!status)
    {
        status.perror("deregisterNode ctColliderPrimitive");
    }


    status = plugin.deregisterNode(ColliderHullNode::id);
    if (!status)
    {
        status.perror("deregisterNode ctColliderHull");
    }
    return status;
}

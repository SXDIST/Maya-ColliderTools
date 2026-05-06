//
// DDConvexHullPlugin.cpp
// DDConvexHull
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

#include <maya/MFnPlugin.h>
#include <maya/MPxManipContainer.h>
#include "DDConvexHullNode.h"
#include "ColliderCreateCmd.h"
#include "ColliderPrimitiveManip.h"
#include "ColliderPrimitiveNode.h"

MStatus initializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj, "ColliderTools", "0.1.0", "Any");
    status = plugin.registerNode("DDConvexHull",
                                 DDConvexHullNode::id,
                                 DDConvexHullNode::creator,
                                 DDConvexHullNode::initialize);
    if (!status)
    {
        status.perror("registerNode DDConvexHull");
        return status;
    }


    status = plugin.registerNode(ColliderPrimitiveNode::typeName,
                                 ColliderPrimitiveNode::id,
                                 ColliderPrimitiveNode::creator,
                                 ColliderPrimitiveNode::initialize);
    if (!status)
    {
        status.perror("registerNode ctColliderPrimitive");
        plugin.deregisterNode(DDConvexHullNode::id);
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
        plugin.deregisterNode(DDConvexHullNode::id);
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
        plugin.deregisterNode(DDConvexHullNode::id);
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


    status = plugin.deregisterNode(DDConvexHullNode::id);
    if (!status)
    {
        status.perror("deregisterNode DDConvexHull");
    }
    return status;
}

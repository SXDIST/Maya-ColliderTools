#ifndef HULL_H
#define HULL_H

#include <vector>

namespace ColliderTools
{
namespace HullCore
{

struct HullRequest
{
    const double* vertices = 0;
    unsigned int vertexCount = 0;
    unsigned int vertexStride = 0;
    unsigned int maxVertices = 4096;
    double normalEpsilon = 0.001;
    double skinWidth = 0.01;
    bool forceTriangles = true;
    bool useSkinWidth = false;
    bool reverseTriangleOrder = false;
};

struct HullMesh
{
    bool polygons = true;
    unsigned int vertexCount = 0;
    unsigned int faceCount = 0;
    std::vector<double> vertices;
    std::vector<unsigned int> indices;
};

bool computeHull(const HullRequest& request, HullMesh& mesh);

}
}

#endif

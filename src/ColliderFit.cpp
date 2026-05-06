#include "ColliderFit.h"

#include "DDConvexHullUtils.h"

#include <maya/MFnMesh.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MTransformationMatrix.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

namespace ColliderTools
{
namespace
{
MVector pointToVector(const MPoint& point)
{
    return MVector(point.x, point.y, point.z);
}

MPoint vectorToPoint(const MVector& value)
{
    return MPoint(value.x, value.y, value.z);
}

long long quantize(double value)
{
    return static_cast<long long>(std::llround(value * 1000000.0));
}

struct QuantizedPoint
{
    long long x;
    long long y;
    long long z;

    bool operator==(const QuantizedPoint& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct QuantizedPointHash
{
    std::size_t operator()(const QuantizedPoint& point) const
    {
        std::size_t seed = std::hash<long long>{}(point.x);
        seed ^= std::hash<long long>{}(point.y) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        seed ^= std::hash<long long>{}(point.z) + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        return seed;
    }
};

MStatus appendMeshPoints(const MDagPath& inputPath, const MObject& component, MPointArray& points)
{
    MStatus status;
    MDagPath meshPath(inputPath);
    if (meshPath.apiType() != MFn::kMesh)
    {
        status = meshPath.extendToShape();
        if (!status)
        {
            return status;
        }
    }

    if (!meshPath.hasFn(MFn::kMesh))
    {
        return MStatus::kInvalidParameter;
    }

    MFnMesh meshFn(meshPath, &status);
    if (!status)
    {
        return status;
    }

    MPointArray meshPoints;
    status = meshFn.getPoints(meshPoints, MSpace::kWorld);
    if (!status)
    {
        return status;
    }

    if (component.isNull())
    {
        for (unsigned int i = 0; i < meshPoints.length(); ++i)
        {
            points.append(meshPoints[i]);
        }
        return MStatus::kSuccess;
    }

    MIntArray indices;
    status = DDConvexHullUtils::componentToVertexIDs(indices, meshPath.node(), component);
    if (!status)
    {
        return status;
    }

    for (unsigned int i = 0; i < indices.length(); ++i)
    {
        const int index = indices[i];
        if (index >= 0 && static_cast<unsigned int>(index) < meshPoints.length())
        {
            points.append(meshPoints[index]);
        }
    }
    return MStatus::kSuccess;
}

MVector centroid(const MPointArray& points)
{
    MVector result(0.0, 0.0, 0.0);
    for (unsigned int i = 0; i < points.length(); ++i)
    {
        result += pointToVector(points[i]);
    }
    return points.length() > 0 ? result / static_cast<double>(points.length()) : MVector::zero;
}

void covariance(const MPointArray& points, const MVector& mean, double out[3][3])
{
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            out[r][c] = 0.0;
        }
    }

    if (points.length() == 0)
    {
        return;
    }

    for (unsigned int i = 0; i < points.length(); ++i)
    {
        const MVector p = pointToVector(points[i]) - mean;
        const double v[3] = {p.x, p.y, p.z};
        for (int r = 0; r < 3; ++r)
        {
            for (int c = 0; c < 3; ++c)
            {
                out[r][c] += v[r] * v[c];
            }
        }
    }

    const double invCount = 1.0 / static_cast<double>(points.length());
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            out[r][c] *= invCount;
        }
    }
}

MVector multiply(const double matrix[3][3], const MVector& value)
{
    return MVector(
        matrix[0][0] * value.x + matrix[0][1] * value.y + matrix[0][2] * value.z,
        matrix[1][0] * value.x + matrix[1][1] * value.y + matrix[1][2] * value.z,
        matrix[2][0] * value.x + matrix[2][1] * value.y + matrix[2][2] * value.z);
}

MVector dominantEigenVector(const double matrix[3][3], const MVector& seed)
{
    MVector vector = safeNormal(seed, MVector::xAxis);
    for (int i = 0; i < 32; ++i)
    {
        vector = safeNormal(multiply(matrix, vector), vector);
    }
    return vector;
}

void stabilizeAxes(MVector axes[3])
{
    for (int axis = 0; axis < 3; ++axis)
    {
        int dominant = 0;
        double dominantValue = std::abs(axes[axis].x);
        if (std::abs(axes[axis].y) > dominantValue)
        {
            dominant = 1;
            dominantValue = std::abs(axes[axis].y);
        }
        if (std::abs(axes[axis].z) > dominantValue)
        {
            dominant = 2;
        }

        const double value = dominant == 0 ? axes[axis].x : (dominant == 1 ? axes[axis].y : axes[axis].z);
        if (value < 0.0)
        {
            axes[axis] *= -1.0;
        }
    }
    if ((axes[0] ^ axes[1]) * axes[2] < 0.0)
    {
        axes[2] *= -1.0;
    }
}

void projectFit(const MPointArray& points, MVector axes[3], BoxFit& fit)
{
    stabilizeAxes(axes);
    double minValues[3] = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max()
    };
    double maxValues[3] = {
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max()
    };

    for (unsigned int i = 0; i < points.length(); ++i)
    {
        const MVector p = pointToVector(points[i]);
        for (int axis = 0; axis < 3; ++axis)
        {
            const double value = p * axes[axis];
            minValues[axis] = std::min(minValues[axis], value);
            maxValues[axis] = std::max(maxValues[axis], value);
        }
    }

    fit.center = MVector::zero;
    for (int axis = 0; axis < 3; ++axis)
    {
        fit.axes[axis] = axes[axis];
        fit.halfExtents[axis] = std::max((maxValues[axis] - minValues[axis]) * 0.5, 1.0e-4);
        fit.center += axes[axis] * ((minValues[axis] + maxValues[axis]) * 0.5);
    }
}
}

MStatus gatherSelectionPointSets(const MSelectionList& selection, std::vector<PointSet>& pointSets)
{
    MStatus status;
    for (MItSelectionList it(selection); !it.isDone(); it.next())
    {
        MDagPath path;
        MObject component;
        status = it.getDagPath(path, component);
        if (!status)
        {
            continue;
        }

        PointSet set;
        set.sourcePath = path;
        set.component = component;
        status = appendMeshPoints(path, component, set.points);
        set.points = cleanPointSet(set.points);
        if (status && set.points.length() > 0)
        {
            pointSets.push_back(set);
        }
    }
    return pointSets.empty() ? MStatus::kInvalidParameter : MStatus::kSuccess;
}

MStatus gatherCombinedSelectionPoints(const MSelectionList& selection, MPointArray& points)
{
    std::vector<PointSet> sets;
    MStatus status = gatherSelectionPointSets(selection, sets);
    if (!status)
    {
        return status;
    }

    for (const PointSet& set : sets)
    {
        for (unsigned int i = 0; i < set.points.length(); ++i)
        {
            points.append(set.points[i]);
        }
    }
    points = cleanPointSet(points);
    return points.length() > 0 ? MStatus::kSuccess : MStatus::kInvalidParameter;
}

MPointArray cleanPointSet(const MPointArray& points)
{
    MPointArray result;
    std::unordered_set<QuantizedPoint, QuantizedPointHash> seen;
    for (unsigned int i = 0; i < points.length(); ++i)
    {
        const MPoint& point = points[i];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
        {
            continue;
        }

        const QuantizedPoint key{quantize(point.x), quantize(point.y), quantize(point.z)};
        if (seen.insert(key).second)
        {
            result.append(point);
        }
    }
    return result;
}

BoxFit fitAabb(const MPointArray& points)
{
    MVector axes[3] = {MVector::xAxis, MVector::yAxis, MVector::zAxis};
    BoxFit fit;
    projectFit(points, axes, fit);
    return fit;
}

BoxFit fitObb(const MPointArray& points)
{
    const MVector mean = centroid(points);
    double cov[3][3];
    covariance(points, mean, cov);

    MVector first = dominantEigenVector(cov, MVector::xAxis);
    MVector secondSeed = std::abs(first * MVector::yAxis) < 0.9 ? MVector::yAxis : MVector::zAxis;
    MVector second = safeNormal(secondSeed - first * (secondSeed * first), MVector::yAxis);
    for (int i = 0; i < 16; ++i)
    {
        MVector candidate = multiply(cov, second);
        candidate -= first * (candidate * first);
        second = safeNormal(candidate, second);
    }
    MVector third = safeNormal(first ^ second, MVector::zAxis);
    second = safeNormal(third ^ first, MVector::yAxis);

    std::vector<MVector> candidatePrimaryAxes;
    candidatePrimaryAxes.push_back(first);
    candidatePrimaryAxes.push_back(second);
    candidatePrimaryAxes.push_back(third);
    candidatePrimaryAxes.push_back(MVector::xAxis);
    candidatePrimaryAxes.push_back(MVector::yAxis);
    candidatePrimaryAxes.push_back(MVector::zAxis);

    std::vector<MVector> representativePoints;
    const MVector probeAxes[6] = {first, second, third, MVector::xAxis, MVector::yAxis, MVector::zAxis};
    for (const MVector& probe : probeAxes)
    {
        double minValue = std::numeric_limits<double>::max();
        double maxValue = -std::numeric_limits<double>::max();
        MVector minPoint;
        MVector maxPoint;
        for (unsigned int i = 0; i < points.length(); ++i)
        {
            const MVector point = pointToVector(points[i]);
            const double value = point * probe;
            if (value < minValue)
            {
                minValue = value;
                minPoint = point;
            }
            if (value > maxValue)
            {
                maxValue = value;
                maxPoint = point;
            }
        }
        representativePoints.push_back(minPoint);
        representativePoints.push_back(maxPoint);
    }

    const unsigned int sampleLimit = 96;
    const unsigned int stride = std::max(1u, points.length() / sampleLimit);
    for (unsigned int i = 0; i < points.length(); i += stride)
    {
        representativePoints.push_back(pointToVector(points[i]));
    }

    for (unsigned int i = 0; i < representativePoints.size(); ++i)
    {
        for (unsigned int j = i + 1; j < representativePoints.size(); ++j)
        {
            const MVector edge = representativePoints[j] - representativePoints[i];
            if (edge.length() > 1.0e-6)
            {
                candidatePrimaryAxes.push_back(safeNormal(edge, first));
            }
        }
    }

    BoxFit bestFit;
    MVector initialAxes[3] = {first, second, third};
    projectFit(points, initialAxes, bestFit);
    double bestVolume = bestFit.halfExtents[0] * bestFit.halfExtents[1] * bestFit.halfExtents[2];

    constexpr int kTwistSamples = 24;
    for (const MVector& rawPrimary : candidatePrimaryAxes)
    {
        const MVector primary = safeNormal(rawPrimary, first);
        MVector baseU, baseV, unusedForward;
        buildFrame(primary, baseU, baseV, unusedForward);

        for (int sample = 0; sample < kTwistSamples; ++sample)
        {
            const double angle = 3.14159265358979323846 * static_cast<double>(sample) / static_cast<double>(kTwistSamples);
            const MVector u = safeNormal(baseU * std::cos(angle) + baseV * std::sin(angle), baseU);
            const MVector v = safeNormal(primary ^ u, baseV);
            MVector axes[3] = {primary, u, v};

            BoxFit candidateFit;
            projectFit(points, axes, candidateFit);
            const double volume = candidateFit.halfExtents[0] * candidateFit.halfExtents[1] * candidateFit.halfExtents[2];
            if (volume < bestVolume)
            {
                bestVolume = volume;
                bestFit = candidateFit;
            }
        }
    }

    return bestFit;
}

void fitAxisShape(const MPointArray& points, const MVector axes[3], MVector& center, MVector& axis, double& halfLength, double& radius)
{
    std::vector<MVector> candidates;
    candidates.push_back(axes[0]);
    candidates.push_back(axes[1]);
    candidates.push_back(axes[2]);
    candidates.push_back(MVector::xAxis);
    candidates.push_back(MVector::yAxis);
    candidates.push_back(MVector::zAxis);

    std::vector<MVector> representativePoints;
    for (const MVector& probe : candidates)
    {
        double minValue = std::numeric_limits<double>::max();
        double maxValue = -std::numeric_limits<double>::max();
        MVector minPoint;
        MVector maxPoint;
        for (unsigned int i = 0; i < points.length(); ++i)
        {
            const MVector point = pointToVector(points[i]);
            const double value = point * probe;
            if (value < minValue)
            {
                minValue = value;
                minPoint = point;
            }
            if (value > maxValue)
            {
                maxValue = value;
                maxPoint = point;
            }
        }
        representativePoints.push_back(minPoint);
        representativePoints.push_back(maxPoint);
    }

    for (unsigned int i = 0; i < representativePoints.size(); ++i)
    {
        for (unsigned int j = i + 1; j < representativePoints.size(); ++j)
        {
            const MVector edge = representativePoints[j] - representativePoints[i];
            if (edge.length() > 1.0e-6)
            {
                candidates.push_back(safeNormal(edge, axes[0]));
            }
        }
    }

    double bestScore = std::numeric_limits<double>::max();
    for (const MVector& rawCandidate : candidates)
    {
        const MVector candidateAxis = safeNormal(rawCandidate, axes[0]);
        double minValue = std::numeric_limits<double>::max();
        double maxValue = -std::numeric_limits<double>::max();
        MVector lateralMin(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
        MVector lateralMax(-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(), -std::numeric_limits<double>::max());
        MVector lateralSum = MVector::zero;

        MVector right, up, forward;
        buildFrame(candidateAxis, right, up, forward);
        for (unsigned int i = 0; i < points.length(); ++i)
        {
            const MVector point = pointToVector(points[i]);
            const double axial = point * forward;
            minValue = std::min(minValue, axial);
            maxValue = std::max(maxValue, axial);
            const double rightValue = point * right;
            const double upValue = point * up;
            lateralMin.x = std::min(lateralMin.x, rightValue);
            lateralMin.y = std::min(lateralMin.y, upValue);
            lateralMax.x = std::max(lateralMax.x, rightValue);
            lateralMax.y = std::max(lateralMax.y, upValue);
            lateralSum += point - forward * axial;
        }

        const double candidateHalfLength = std::max((maxValue - minValue) * 0.5, 1.0e-4);
        const MVector lateralCenter = points.length() > 0 ? lateralSum / static_cast<double>(points.length()) : MVector::zero;
        double candidateRadius = 1.0e-4;
        for (unsigned int i = 0; i < points.length(); ++i)
        {
            const MVector point = pointToVector(points[i]);
            const double axial = point * forward;
            candidateRadius = std::max(candidateRadius, (point - forward * axial - lateralCenter).length());
        }

        const double lateralWidth = std::max(lateralMax.x - lateralMin.x, lateralMax.y - lateralMin.y);
        const double score = candidateRadius * candidateRadius * candidateHalfLength + lateralWidth * lateralWidth * 1.0e-6;
        if (score < bestScore)
        {
            bestScore = score;
            axis = forward;
            radius = candidateRadius;
            halfLength = candidateHalfLength;
            center = forward * ((minValue + maxValue) * 0.5) + lateralCenter;
        }
    }
}
}

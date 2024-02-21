

#include "Types/DMLandscapeGeneratorTypes.h"

int32 FDMISM::PopIndex()
{
    if (RelocateIndexes.IsEmpty()) return -1;
    return RelocateIndexes.Pop();
}

FTransform FDMISM::PopTransform()
{
    if (QueueTransforms.IsEmpty()) return FTransform();
    return QueueTransforms.Pop();
}

void FDMPlaneTile::SetData(const TArray<FVector> InVertices, const TArray<FVector2D> InUVs,
    const TArray<int32> InTriangles, const TArray<FVector> InNormals, const TArray<FProcMeshTangent> InTangents)
{
    Vertices = InVertices;
    UVs = InUVs;
    Triangles = InTriangles;
    Normals = InNormals;
    Tangents = InTangents;
}

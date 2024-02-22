

#pragma once

#include "CoreMinimal.h"
#include "KismetProceduralMeshLibrary.h"

#include "Types/DMConstants.h"

#include "DMLandscapeGeneratorTypes.generated.h"

class UInstancedStaticMeshComponent;

#pragma region enums

UENUM(BlueprintType)
// Размер
enum class EDMSize : uint8
{
    Size1x1 UMETA(DisplayName = "1x1"),
    Size3x3 UMETA(DisplayName = "3x3"),
    Size5x5 UMETA(DisplayName = "5x5"),
    Size7x7 UMETA(DisplayName = "7x7"),
};

const TMap<EDMSize, uint8> SizeConnection = {
    {EDMSize::Size1x1, 1},
    {EDMSize::Size3x3, 3},
    {EDMSize::Size5x5, 5},
    {EDMSize::Size7x7, 7},
};

UENUM(BlueprintType)
// Размер в метрах
enum class EDMTileSize : uint8
{
    Size5x5m UMETA(DisplayName = "5x5 m"),
    Size10x10m UMETA(DisplayName = "10x10 m"),
    Size15x15m UMETA(DisplayName = "15x15 m"),
    Size20x20m UMETA(DisplayName = "20x20 m"),
    Size25x25m UMETA(DisplayName = "25x25 m"),
    Size30x30m UMETA(DisplayName = "30x30 m"),
    Size40x40m UMETA(DisplayName = "40x40 m"),
    Size50x50m UMETA(DisplayName = "50x50 m"),
    Size75x75m UMETA(DisplayName = "75x75 m"),

    Size100x100m UMETA(DisplayName = "100x100 m"),
    Size150x150m UMETA(DisplayName = "150x150 m"),
    Size200x200m UMETA(DisplayName = "200x200 m"),
    Size250x250m UMETA(DisplayName = "250x250 m"),
    Size300x300m UMETA(DisplayName = "300x300 m"),
    Size400x400m UMETA(DisplayName = "400x400 m"),
    Size500x500m UMETA(DisplayName = "500x500 m"),
    Size600x600m UMETA(DisplayName = "600x600 m"),
};

const TMap<EDMTileSize, uint16> TileSizeConnection = {
    {EDMTileSize::Size5x5m, 500},
    {EDMTileSize::Size10x10m, 1000},
    {EDMTileSize::Size15x15m, 1500},
    {EDMTileSize::Size20x20m, 2000},
    {EDMTileSize::Size25x25m, 2500},
    {EDMTileSize::Size30x30m, 3000},
    {EDMTileSize::Size40x40m, 4000},
    {EDMTileSize::Size50x50m, 5000},
    {EDMTileSize::Size75x75m, 7500},
    {EDMTileSize::Size100x100m, 10000},
    {EDMTileSize::Size150x150m, 15000},
    {EDMTileSize::Size200x200m, 20000},
    {EDMTileSize::Size250x250m, 25000},
    {EDMTileSize::Size300x300m, 30000},
    {EDMTileSize::Size400x400m, 40000},
    {EDMTileSize::Size500x500m, 50000},
    {EDMTileSize::Size600x600m, 60000},
};

UENUM(BlueprintType)
// Размер в полигонах
enum class EDMCellSize : uint8
{
    Size1x1 UMETA(DisplayName = "1x1 (2 tris)"),
    Size2x2 UMETA(DisplayName = "2x2 (8)"),
    Size4x4 UMETA(DisplayName = "4x4 (32)"),
    Size8x8 UMETA(DisplayName = "8x8 (128)"),
    Size16x16 UMETA(DisplayName = "16x16 (512)"),
    Size32x32 UMETA(DisplayName = "32x32 (2 048)"),
    Size64x64 UMETA(DisplayName = "64x64 (8 192)"),
    Size128x128 UMETA(DisplayName = "128x128 (32 768)"),
};

const TMap<EDMCellSize, uint8> CellSizeConnection = {
    {EDMCellSize::Size1x1, 1},
    {EDMCellSize::Size2x2, 2},
    {EDMCellSize::Size4x4, 4},
    {EDMCellSize::Size8x8, 8},
    {EDMCellSize::Size16x16, 16},
    {EDMCellSize::Size32x32, 32},
    {EDMCellSize::Size64x64, 64},
    {EDMCellSize::Size128x128, 128},
};

// TODO: до лучших времен
enum class EDMLOD : uint8
{
    LOD0 UMETA(DisplayName = "LOD 0"),
    LOD1 UMETA(DisplayName = "LOD 1 (div 2)"),
    LOD2 UMETA(DisplayName = "LOD 2 (div 4)"),
    LOD3 UMETA(DisplayName = "LOD 3 (div 8)"),
    LOD4 UMETA(DisplayName = "LOD 4 (div 16)"),
    LOD5 UMETA(DisplayName = "LOD 5 (div 32)"),
    LOD6 UMETA(DisplayName = "LOD 6 (div 64)"),
    LOD7 UMETA(DisplayName = "LOD 7 (div 128)"),
    LOD8 UMETA(DisplayName = "LOD 8 (div 256)"),
};

enum class EDMAsyncTaskType : uint8
{
    PreparePlane,
    CalculatePlane,
    CalculateFoliage,
    CalculateGrass,
};

// Глобальное состояние расчетов
// Create = Calculate + Spawn
enum class EDMExecutionState : uint8
{
    Idle,
    PreparePlane,
    PreparedPlane,
    CreatePlane,
    CreatedPlane,
    CreateFoliage,
    CreatedFoliage,
    CalculateGrass,
    CalculatedGrass,
    SpawnGrass,
    SpawnedGrass,
};

// Состояние плитки
enum class EDMPlaneTileState : uint8
{
    IgnoreRequest,
    CreateRequest,

    CalculatePlane,
    CalculatedPlane,
    SpawnPlane,
    SpawnedPlane,

    CalculateFoliage,
    CalculatedFoliage,
    SpawnFoliage,
    SpawnedFoliage,
};

#pragma endregion

#pragma region structures

USTRUCT(BlueprintType)
struct FDMPerlinSetting
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Amplitude = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Frequency = .1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Offset = .1f;
};

USTRUCT(BlueprintType)
struct FDMISM
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    UStaticMesh* StaticMesh;

    // Максимальное количество семян на 100 квадратных метров (10 000 см)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    int32 MaxCount = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    float MaxScale = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    float MinScale = .75f;

    // Максимальные углы поворота
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    FRotator Rotation = FRotator(0.f, 360.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    int32 MaxHeight = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    int32 MinHeight = -1000;

    // Максимальный угол наклона
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    int32 MaxSlopeAngle = 60;

    // Минимальный угол наклона
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bEnable"))
    int32 MinSlopeAngle = 0;

public:
    // Получить ISM компоненту
    UInstancedStaticMeshComponent* GetComponent() { return Component; }

    // Установить ISM компоненту
    void SetComponent(UInstancedStaticMeshComponent* InComponent) { Component = InComponent; };

    // Забрать последний свободный индекс. Если нет записей, то -1
    int32 PopIndex();

    // Добавить свободный индекс
    void AddIndex(const int32 Index) { RelocateIndexes.AddUnique(Index); };

    // Забрать преобразование из очереди. Если нет записей, то FTransform()
    FTransform PopTransform();

    // Добавить преобразование в очередь
    void AddTransform(const FTransform Transform) { QueueTransforms.Emplace(Transform); };

private:
    UInstancedStaticMeshComponent* Component;
    TArray<int32> RelocateIndexes;
    TArray<FTransform> QueueTransforms;
};

USTRUCT(BlueprintType)
struct FDMISMWithTiles : public FDMISM
{
    GENERATED_BODY()

public:
    TArray<FIntVector2> Tiles;
};

struct FDMPlaneTile
{
public:
    FDMPlaneTile() = default;
    FDMPlaneTile(const FIntVector2 Tile) : Tile(Tile) {}

    // Получить позицию плитки
    FIntVector2 GetTile() const { return Tile; }

    // Получить состояние плитки
    EDMPlaneTileState GetState() const { return State; }

    // Установить состояние плитки
    void SetState(const EDMPlaneTileState NewStatus) { State = NewStatus; }

    // Получить индекс сетки
    int32 GetMeshSectionIndex() const { return MeshSectionIndex; }

    // Установить индекс сетки
    void SetMeshSectionIndex(const int32 InMeshSectionIndex) { MeshSectionIndex = InMeshSectionIndex; };

    // Получить вершины
    const TArray<FVector>& GetVertices() const { return Vertices; };

    // Получить UV
    const TArray<FVector2D>& GetUVs() const { return UVs; }

    // Получить треугольники
    const TArray<int32>& GetTriangles() const { return Triangles; }

    // Получить нормали
    const TArray<FVector>& GetNormals() const { return Normals; }

    // Получить касательные
    const TArray<FProcMeshTangent>& GetTangents() const { return Tangents; }

    // Установить данные
    void SetData(const TArray<FVector> InVertices, const TArray<FVector2D> InUVs, const TArray<int32> InTriangles,
        const TArray<FVector> InNormals, const TArray<FProcMeshTangent> InTangents);

private:
    const FIntVector2 Tile;
    EDMPlaneTileState State = EDMPlaneTileState::CreateRequest;
    int32 MeshSectionIndex = -1;

    TArray<FVector> Vertices;
    TArray<FVector2D> UVs;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FProcMeshTangent> Tangents;
};

#pragma endregion



#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Types/DMConstants.h"
#include "Types/DMLandscapeGeneratorTypes.h"
#include "Types/DMStringLiterals.h"

#include "DMLandscapeGenerator.generated.h"

/*
 * Landscape (Ландшафт) - весь сгенерированный ландшафт с растительностью (состоит из множества Chunk)
 * Tile (Плитка) - любая плитка поверхности или растительности
 * Cell (Ячейка) - ячейка плитки
 * Plane - поверхность
 * Foliage (Наполнитель) - камни и деревья
 * Planiage (PLANe + folIAGE) - поверхность с камнями и деревьями, но без травы
 * Grass - травка
 */

class UProceduralMeshComponent;
class UBoxComponent;

UCLASS()
class DEMONMIX_API ADMLandscapeGenerator : public AActor
{
    GENERATED_BODY()

public:
    ADMLandscapeGenerator();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane")
    EDMSize Size = EDMSize::Size3x3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane")
    EDMTileSize TileSize = EDMTileSize::Size150x150m;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane")
    EDMCellSize TileCellSize = EDMCellSize::Size64x64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane")
    TArray<FDMPerlinSetting> NoiseSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane", meta = (ClampMin = "1", ClampMax = "2"))
    double TriggerScale = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane")
    float TimerRate = .1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane")
    UMaterialInterface* Material = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plane")
    TArray<FDMISM> Foliages;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    EDMSize GrassSize = EDMSize::Size3x3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    EDMTileSize GrassTileSize = EDMTileSize::Size75x75m;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass", meta = (ClampMin = "1", ClampMax = "2"))
    double GrassTriggerScale = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    TArray<FDMISMWithTiles> Grasses;

    void CalculateAsync(const EDMAsyncTaskType TaskType);

protected:
    virtual void BeginPlay() override;

private:
    APawn* PlayerPawn;
    UBoxComponent* Trigger;
    UBoxComponent* GrassTrigger;
    UProceduralMeshComponent* ProceduralMeshComponent;

    FTimerHandle UpdateTimerHandle;
    FIntVector2 CurrentTile;
    FIntVector2 CurrentGrassTile;
    TArray<FDMPlaneTile> PlaneTiles;

    // Случайное смещение шума
    FVector2D RandomNoiseOffset;

    // Текущее состояние расчета
    EDMExecutionState ExecutionState = EDMExecutionState::Idle;

    // Количество плиток в одной линии ландшафта
    uint8 TileCount = 0;

    // Длина одной стороны плитки
    uint16 TileLen = 0;

    // Количество ячеек в одной линии плитки
    uint8 CellCount = 0;

    // Длина ячейки
    float CellLen = 0.f;

    // Количество плиток травы в одной линии
    uint8 GrassCount = 0;

    // Длина одной стороны плитки травы
    uint16 GrassLen = 0;

    bool bUpdatePlaniageRequest = false;
    bool bUpdateGrassRequest = false;
    bool bAsyncTaskIsBusy = false;

    UFUNCTION()
    void OnTrigerOut(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    void UpdateByTimer();
    void PreparePlaneAsync();
    void CalculatePlaneAsync();
    void CalculateFoliageAsync();
    void CalculateGrassAsync();

    float CalculateZ(const FVector2D Location) const;
    bool CalculateInstancesTransform(FDMISM& ISM, const FIntVector2 Start, const FIntVector2 End);

    void StartAsyncTask(const EDMAsyncTaskType TaskType);

    void StartTimer();
    void StopTimer();

    // TODO: убрать пока не запахло
    bool ContainsPlaneByState(const EDMPlaneTileState State) const;
    bool ContainsPlaneByMeshSectionIndex(const int32 MeshSectionIndex) const;
    FDMPlaneTile* FindPlaneByState(const EDMPlaneTileState State);
    FDMPlaneTile* FindPlaneByTile(const FIntVector2 Tile);
};

// Assинхронный расчет значений
class FDMCalculationAsync : public FNonAbandonableTask
{
public:
    FDMCalculationAsync(ADMLandscapeGenerator* LandscapeGenerator, const EDMAsyncTaskType TaskType)
        : LandscapeGenerator(LandscapeGenerator), TaskType(TaskType)
    {
    }
    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FDMCalculationAsync, STATGROUP_ThreadPoolAsyncTasks)
    }
    void DoWork() { LandscapeGenerator->CalculateAsync(TaskType); }

private:
    ADMLandscapeGenerator* LandscapeGenerator;
    const EDMAsyncTaskType TaskType;
};

// TODO: вынести когда-нибудь куда-нибудь
void PickupFreeInstances(FDMISM& InISM, const FIntVector2 CurrentTile, const FIntVector2 PrevTile,
    const uint8 SaveRadius, const uint16 TileLen);
FIntVector2 GetTileByLocation(const FVector Location, const uint16 TileLen);
bool IsExternalTile(const FIntVector2 CheckedTile, const FIntVector2 CenterTile, const uint8 SaveRadius);

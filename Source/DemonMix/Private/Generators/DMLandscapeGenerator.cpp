

#include "Generators/DMLandscapeGenerator.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMLandscapeGenerator, All, All);

ADMLandscapeGenerator::ADMLandscapeGenerator()
{
    ProceduralMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(LandscapeGenerator::ProceduralMesh);
    ProceduralMeshComponent->SetupAttachment(GetRootComponent());
    ProceduralMeshComponent->bUseAsyncCooking = true;

    Trigger = CreateDefaultSubobject<UBoxComponent>(LandscapeGenerator::Trigger);
    Trigger->SetupAttachment(GetRootComponent());
    Trigger->SetGenerateOverlapEvents(true);

    GrassTrigger = CreateDefaultSubobject<UBoxComponent>(LandscapeGenerator::GrassTrigger);
    GrassTrigger->SetupAttachment(GetRootComponent());
    GrassTrigger->SetGenerateOverlapEvents(true);
}

void ADMLandscapeGenerator::BeginPlay()
{
    Super::BeginPlay();

    PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), Common::DefaultPlayerIndex);
    check(PlayerPawn);

    TileCount = *SizeConnection.Find(Size);
    check(TileCount > 0);

    TileLen = *TileSizeConnection.Find(TileSize);
    check(TileLen > 0);

    CellCount = *CellSizeConnection.Find(TileCellSize);
    check(CellCount > 0);

    CellLen = (float)TileLen / CellCount;

    GrassCount = *SizeConnection.Find(GrassSize);
    check(GrassCount > 0);

    GrassLen = *TileSizeConnection.Find(GrassTileSize);
    check(TileLen > 0);

    RandomNoiseOffset = FVector2D( //
        FMath::FRandRange(-LandscapeGenerator::RandomNoiseRange, LandscapeGenerator::RandomNoiseRange),
        FMath::FRandRange(-LandscapeGenerator::RandomNoiseRange, LandscapeGenerator::RandomNoiseRange));

    // триггер поверхности
    const float PlaneTriggerLen = TileLen * TriggerScale * .5f;
    Trigger->SetBoxExtent(FVector(PlaneTriggerLen, PlaneTriggerLen, LandscapeGenerator::TriggerHeight));
    Trigger->OnComponentEndOverlap.AddDynamic(this, &ADMLandscapeGenerator::OnTrigerOut);

    // наполнитель
    for (FDMISM& Foliage : Foliages)
    {
        // заполни пустые фолиэйджи, брат
        check(Foliage.StaticMesh);

        UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(this);
        ISMComponent->RegisterComponent();
        ISMComponent->SetStaticMesh(Foliage.StaticMesh);
        ISMComponent->SetFlags(RF_Transactional);
        this->AddInstanceComponent(ISMComponent);
        Foliage.SetComponent(ISMComponent);
    }

    // травка
    for (FDMISM& Grass : Grasses)
    {
        // где травка, брат?
        check(Grass.StaticMesh);

        UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(this);
        ISMComponent->RegisterComponent();
        ISMComponent->SetStaticMesh(Grass.StaticMesh);
        ISMComponent->SetFlags(RF_Transactional);
        this->AddInstanceComponent(ISMComponent);
        Grass.SetComponent(ISMComponent);
    }

    // триггер травы
    const float GrassTriggerLen = GrassLen * GrassTriggerScale * .5f;
    GrassTrigger->SetBoxExtent(FVector(GrassTriggerLen, GrassTriggerLen, LandscapeGenerator::TriggerHeight));
    GrassTrigger->OnComponentEndOverlap.AddDynamic(this, &ADMLandscapeGenerator::OnTrigerOut);

    // первый расчет
    bUpdatePlaniageRequest = true;
    bUpdateGrassRequest = true;
    StartTimer();
}

void ADMLandscapeGenerator::CalculateAsync(const EDMAsyncTaskType TaskType)
{
    switch (TaskType)
    {
        case EDMAsyncTaskType::PreparePlane:
            PreparePlaneAsync();
            break;

        case EDMAsyncTaskType::CalculatePlane:
            CalculatePlaneAsync();
            break;

        case EDMAsyncTaskType::CalculateFoliage:
            CalculateFoliageAsync();
            break;

        case EDMAsyncTaskType::CalculateGrass:
            CalculateGrassAsync();
            break;
    }

    bAsyncTaskIsBusy = false;
}

void ADMLandscapeGenerator::OnTrigerOut(
    UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (OtherActor != PlayerPawn) return;

    if (OverlappedComponent == Trigger)
    {
        bUpdatePlaniageRequest = true;
    }
    else if (OverlappedComponent == GrassTrigger)
    {
        bUpdateGrassRequest = true;
    }
    else
    {
        // кто я, брат?
        check(0);
    }

    StartTimer();
}

void ADMLandscapeGenerator::UpdateByTimer()
{
    // TODO: встречается дублирование кода
    // состояния меняются последовательно
    switch (ExecutionState)
    {
        case EDMExecutionState::Idle:
        {
            if (bUpdatePlaniageRequest)
            {
                ExecutionState = EDMExecutionState::PreparePlane;
                bUpdatePlaniageRequest = false;
            }
            else if (bUpdateGrassRequest)
            {
                ExecutionState = EDMExecutionState::CalculateGrass;
                bUpdateGrassRequest = false;
            }
            else
            {
                // для чего я тут, брат?
                check(0);
            }
        }
        break;

        case EDMExecutionState::PreparePlane:
        {
            if (bAsyncTaskIsBusy) return;

            // получаем текущую плитку
            const FIntVector2 PrevTile = CurrentTile;
            CurrentTile = GetTileByLocation(PlayerPawn->GetActorLocation(), TileLen);

            UE_LOG(LogDMLandscapeGenerator, Display, TEXT("Player tile [%d : %d]"), CurrentTile.X, CurrentTile.Y);

            // перемещаем триггер в центр новой плитки
            Trigger->SetWorldLocation(FVector(CurrentTile.X + .5f, CurrentTile.Y + .5f, 0.f) * TileLen);

            // TODO: можно вынести
            // удаляем старые плитки
            const uint8 TileRadius = (TileCount - 1) / 2;
            for (int16 iTile = PlaneTiles.Num() - 1; iTile >= 0; --iTile)
            {
                const FIntVector2 Tile = PlaneTiles[iTile].GetTile();
                if (IsExternalTile(Tile, CurrentTile, TileRadius))
                {
                    const int32 MeshSectionIndex = PlaneTiles[iTile].GetMeshSectionIndex();
                    PlaneTiles.RemoveAt(iTile);

                    if (MeshSectionIndex != -1) ProceduralMeshComponent->ClearMeshSection(MeshSectionIndex);
                }
            }

            // запускаем ассинхронную подготовку данных
            StartAsyncTask(EDMAsyncTaskType::PreparePlane);

            // подбираем освободившиеся инстансы наполнителя
            for (FDMISM& Foliage : Foliages)
            {
                PickupFreeInstances(Foliage, CurrentTile, PrevTile, TileRadius, TileLen);
            }
        }
        break;

        case EDMExecutionState::PreparedPlane:
        {
            // ...

            ExecutionState = EDMExecutionState::CreatePlane;
        } // break;

        case EDMExecutionState::CreatePlane:
        {
            // запускаем задачу, если свободно
            const bool bHasCreateRequest = ContainsPlaneByState(EDMPlaneTileState::CreateRequest);
            if (bHasCreateRequest)
            {
                StartAsyncTask(EDMAsyncTaskType::CalculatePlane);
            }

            // TODO: (2) хз как работают таймеры, и нужна ли эта проверка
            // если плитка создается, то пропускаем ход
            const bool bHasSpawnPlane = ContainsPlaneByState(EDMPlaneTileState::SpawnPlane);
            if (bHasSpawnPlane)
            {
                return;
            }

            // создаем плитку
            FDMPlaneTile* CalculatedPlaneTile = FindPlaneByState(EDMPlaneTileState::CalculatedPlane);
            if (CalculatedPlaneTile)
            {
                CalculatedPlaneTile->SetState(EDMPlaneTileState::SpawnPlane);

                ProceduralMeshComponent->CreateMeshSection(CalculatedPlaneTile->GetMeshSectionIndex(),
                    CalculatedPlaneTile->GetVertices(), CalculatedPlaneTile->GetTriangles(),
                    CalculatedPlaneTile->GetNormals(), CalculatedPlaneTile->GetUVs(), TArray<FColor>(),
                    CalculatedPlaneTile->GetTangents(), true);

                if (Material)
                {
                    ProceduralMeshComponent->SetMaterial(CalculatedPlaneTile->GetMeshSectionIndex(), Material);
                }

                CalculatedPlaneTile->SetState(EDMPlaneTileState::SpawnedPlane);
            }

            if (bAsyncTaskIsBusy ||  // если задача что-то делает
                bHasCreateRequest || // если есть запрос на создание
                bHasSpawnPlane ||    // если есть плитка которая создается
                CalculatedPlaneTile) // если плитка была создана в этой итерации
            {
                return;
            }

            ExecutionState = EDMExecutionState::CreatedPlane;
        } // break;

        case EDMExecutionState::CreatedPlane:
        {
            // если есть запрос на обновление ландшафта, то переходим на первый этап
            if (bUpdatePlaniageRequest)
            {
                ExecutionState = EDMExecutionState::PreparePlane;
                bUpdatePlaniageRequest = false;
                return;
            }

            ExecutionState = EDMExecutionState::CreateFoliage;
        } // break;

        case EDMExecutionState::CreateFoliage:
        {
            // запускаем задачу, если свободно
            const bool bHasSpawnedPlane = ContainsPlaneByState(EDMPlaneTileState::SpawnedPlane);
            if (bHasSpawnedPlane)
            {
                StartAsyncTask(EDMAsyncTaskType::CalculateFoliage);
            }

            // TODO: (2)
            // если наполнитель создается, то пропускаем ход
            const bool bHasSpawnFoliagePlane = ContainsPlaneByState(EDMPlaneTileState::SpawnFoliage);
            if (bHasSpawnFoliagePlane)
            {
                return;
            }

            // создаем наполнитель
            FDMPlaneTile* CalculatedFoliagePlaneTile = FindPlaneByState(EDMPlaneTileState::CalculatedFoliage);
            if (CalculatedFoliagePlaneTile)
            {
                CalculatedFoliagePlaneTile->SetState(EDMPlaneTileState::SpawnFoliage);

                for (FDMISM& Foliage : Foliages)
                {
                    FTransform InstanceTransform;
                    while (!(InstanceTransform = Foliage.PopTransform()).Equals(FTransform()))
                    {
                        // TODO: (3) избавиться от костыля позже
                        if (!InstanceTransform.IsRotationNormalized()) InstanceTransform.NormalizeRotation();

                        const int32 ISMIndex = Foliage.PopIndex();
                        if (ISMIndex == -1)
                        {
                            Foliage.GetComponent()->AddInstance(InstanceTransform, true);
                        }
                        else
                        {
                            Foliage.GetComponent()->UpdateInstanceTransform(ISMIndex, InstanceTransform, true, true);
                        }
                    }
                }

                CalculatedFoliagePlaneTile->SetState(EDMPlaneTileState::SpawnedFoliage);
            }

            if (bAsyncTaskIsBusy || // если задача что-то делает
                bHasSpawnedPlane || // если есть созданная плитка без наполнителя
                bHasSpawnFoliagePlane || // если есть плитка, в которой создается наполнитель
                CalculatedFoliagePlaneTile) // если плитка была наполнена в этой итерации
            {
                return;
            }

            ExecutionState = EDMExecutionState::CreatedFoliage;
        } // break;

        case EDMExecutionState::CreatedFoliage:
        {
            // удалям оставшиеся инстансы наполнителя
            for (FDMISM& Foliage : Foliages)
            {
                int32 ISMIndex;
                while ((ISMIndex = Foliage.PopIndex()) != -1)
                {
                    // TODO: (1)
                    Foliage.GetComponent()->RemoveInstance(ISMIndex);
                }
            }

            ExecutionState = EDMExecutionState::Idle;
            StopTimer();
        }
        break;

        case EDMExecutionState::CalculateGrass:
        {
            if (bAsyncTaskIsBusy) return;

            // получаем текущую плитку травы
            const FIntVector2 PrevGrassTile = CurrentGrassTile;
            CurrentGrassTile = GetTileByLocation(PlayerPawn->GetActorLocation(), GrassLen);

            UE_LOG(LogDMLandscapeGenerator, Display, //
                TEXT("Player grass tile [%d : %d]"), CurrentGrassTile.X, CurrentGrassTile.Y);

            // перемещаем триггер травы в центр новой плитки
            GrassTrigger->SetWorldLocation(FVector(CurrentGrassTile.X + .5f, CurrentGrassTile.Y + .5f, 0.f) * GrassLen);

            // запускаем расчет травы
            StartAsyncTask(EDMAsyncTaskType::CalculateGrass);

            // подбираем освободившуюся траву
            const uint8 GrassTileRadius = (GrassCount - 1) / 2;
            for (FDMISMWithTiles& Grass : Grasses)
            {
                PickupFreeInstances(Grass, CurrentGrassTile, PrevGrassTile, GrassTileRadius, GrassLen);
            }
        }
        break;

        case EDMExecutionState::CalculatedGrass:
        {
            // ...

            ExecutionState = EDMExecutionState::SpawnGrass;
        } // break;

        case EDMExecutionState::SpawnGrass:
        {
            for (FDMISMWithTiles& Grass : Grasses)
            {
                FTransform InstanceTransform;
                while (!(InstanceTransform = Grass.PopTransform()).Equals(FTransform()))
                {
                    // TODO: (3)
                    if (!InstanceTransform.IsRotationNormalized()) InstanceTransform.NormalizeRotation();

                    const int32 ISMIndex = Grass.PopIndex();
                    if (ISMIndex == -1)
                    {
                        Grass.GetComponent()->AddInstance(InstanceTransform, true);
                    }
                    else
                    {
                        Grass.GetComponent()->UpdateInstanceTransform(ISMIndex, InstanceTransform, true, true);
                    }
                }
            }

            ExecutionState = EDMExecutionState::SpawnedGrass;
        } // break;

        case EDMExecutionState::SpawnedGrass:
        {
            // удалям оставшиеся инстансы травы
            for (FDMISMWithTiles& Grass : Grasses)
            {
                int32 ISMIndex;
                while ((ISMIndex = Grass.PopIndex()) != -1)
                {
                    Grass.GetComponent()->RemoveInstance(ISMIndex);
                }
            }

            ExecutionState = EDMExecutionState::Idle;
            StopTimer();
        }
        break;
    }
}

void ADMLandscapeGenerator::PreparePlaneAsync()
{
    // рассчитываем состояния плиток для текущего положения
    const uint8 TileRadius = (TileCount - 1) / 2;
    for (int8 iX = -TileRadius; iX <= TileRadius; ++iX)
    {
        for (int8 iY = -TileRadius; iY <= TileRadius; ++iY)
        {
            const FIntVector2 Tile = FIntVector2(CurrentTile.X + iX, CurrentTile.Y + iY);
            FDMPlaneTile* PlaneTile = FindPlaneByTile(Tile);

            // если плитка создана, игнорируем ее
            if (PlaneTile)
            {
                PlaneTile->SetState(EDMPlaneTileState::IgnoreRequest);
            }
            // если нет, тогда добавляем в очередь на обработку
            else
            {
                // позже нужно обязательно установить MeshSectionIndex
                PlaneTiles.Emplace(FDMPlaneTile(Tile));
            }
        }
    }

    // сначала идут ближние к игроку с состоянием CreateRequest
    PlaneTiles.Sort(
        [this](const FDMPlaneTile& A, const FDMPlaneTile& B)
        {
            // A - создание, B - НЕ создание: НЕ меняем порядок
            if (A.GetState() == EDMPlaneTileState::CreateRequest && B.GetState() != EDMPlaneTileState::CreateRequest)
            {
                return true;
            }

            // A - НЕ создание, B - создание: меняем порядок
            if (A.GetState() != EDMPlaneTileState::CreateRequest && B.GetState() == EDMPlaneTileState::CreateRequest)
            {
                return false;
            }

            // A - НЕ создание, B - НЕ создание: НЕ меняем порядок
            if (A.GetState() != EDMPlaneTileState::CreateRequest && B.GetState() != EDMPlaneTileState::CreateRequest)
            {
                return true;
            }

            const uint8 AX = FMath::Abs(CurrentTile.X - A.GetTile().X);
            const uint8 AY = FMath::Abs(CurrentTile.Y - A.GetTile().Y);
            const uint8 BX = FMath::Abs(CurrentTile.X - B.GetTile().X);
            const uint8 BY = FMath::Abs(CurrentTile.Y - B.GetTile().Y);
            const uint8 AXY = AX + AY;
            const uint8 BXY = BX + BY;

            // [1] выбираем минимальную длину
            // [2] длина может совпадать у крайней (2:0) и диагональной (1:1),
            // поэтому берем вариант, где разность по модулю меньше
            return AXY < BXY ||                                             // [1]
                   AXY == BXY && FMath::Abs(AX - AY) < FMath::Abs(BX - BY); // [2]
        });

    ExecutionState = EDMExecutionState::PreparedPlane;
}

// TODO: проверить расчеты еще раз
void ADMLandscapeGenerator::CalculatePlaneAsync()
{
    FDMPlaneTile* PlaneTile = FindPlaneByState(EDMPlaneTileState::CreateRequest);
    if (!PlaneTile) return;

    PlaneTile->SetState(EDMPlaneTileState::CalculatePlane);

    // расширенная плитка
    TArray<FVector> VerticesExt;
    TArray<FVector2D> UVsExt;
    TArray<int32> TrianglesExt;
    TArray<FVector> NormalsExt;
    TArray<FProcMeshTangent> TangentsExt;

    // нормальная плитка
    TArray<FVector> Vertices;
    TArray<FVector2D> UVs;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FProcMeshTangent> Tangents;

    // берем на 1 круг больше, чтобы рассчитать нормали в месте стыка
    const int32 CellCountInLineExt = CellCount + 2;
    const int32 VertexCountInLineExt = CellCountInLineExt + 1;
    for (int32 iCellX = -1; iCellX < CellCountInLineExt; ++iCellX)
    {
        for (int32 iCellY = -1; iCellY < CellCountInLineExt; ++iCellY)
        {
            // вершины
            const float LocationX = iCellX * CellLen + PlaneTile->GetTile().X * TileLen;
            const float LocationY = iCellY * CellLen + PlaneTile->GetTile().Y * TileLen;
            const float LocationZ = CalculateZ(FVector2D(LocationX, LocationY));
            VerticesExt.Emplace(FVector(LocationX, LocationY, LocationZ));

            // TODO: с UV бывают проблемы в виде полос + сетка неравномерная:
            // на 51 плитке при: [3x3] [40x40m] [32x32]
            // на 25 плитке при: [3x3] [40x40m] [64x64]
            // на 25 плитке при: [3x3] [20x20m] [64x64]
            // на 25 плитке при: [3x3] [10x10m] [64x64]
            // на 10 плитке при: [3x3] [25x25m] [128x128]
            // на 10 плитке при: [5x5] [25x25m] [128x128]
            // геометрия накладывается на геометрию или неправильная формула расчета UV? ошибка в приведении типов?
            // TODO: нужно ли каждый раз рассчитывать UV?
            const float UVX = (iCellX + PlaneTile->GetTile().X * CellCount) * CellLen / 100;
            const float UVY = (iCellY + PlaneTile->GetTile().Y * CellCount) * CellLen / 100;
            UVsExt.Emplace(FVector2D(UVX, UVY));
        }
    }

    // расширенная поверхность для определения нормалей по краям
    for (int32 iVertexX = 0; iVertexX < CellCountInLineExt; ++iVertexX)
    {
        for (int32 iVertexY = 0; iVertexY < CellCountInLineExt; ++iVertexY)
        {
            TrianglesExt.Emplace(iVertexX + VertexCountInLineExt * iVertexY);
            TrianglesExt.Emplace(iVertexX + VertexCountInLineExt * iVertexY + 1);
            TrianglesExt.Emplace(iVertexX + VertexCountInLineExt * (iVertexY + 1));
        }
    }

    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(VerticesExt, TrianglesExt, UVsExt, NormalsExt, TangentsExt);

    // не учитываем крайние вершины
    for (int32 iVertex = VertexCountInLineExt + 1;           // не учитываем первую строку
         iVertex < VerticesExt.Num() - VertexCountInLineExt; // не учитываем последнюю строку
         ++iVertex)
    {
        // не учитываем вершины по сторонам
        const int32 Modulo = iVertex % VertexCountInLineExt;
        if (Modulo == 0 || Modulo == VertexCountInLineExt - 1) continue;

        Vertices.Emplace(VerticesExt[iVertex]);
        UVs.Emplace(UVsExt[iVertex]);
        Normals.Emplace(NormalsExt[iVertex]);
        Tangents.Emplace(TangentsExt[iVertex]);
    }

    // строим основную поверхность
    const int32 VertexCountInLine = CellCount + 1;
    for (int32 iVertexX = 0; iVertexX < CellCount; ++iVertexX)
    {
        for (int32 iVertexY = 0; iVertexY < CellCount; ++iVertexY)
        {
            Triangles.Emplace(iVertexX + VertexCountInLine * iVertexY);
            Triangles.Emplace(iVertexX + VertexCountInLine * iVertexY + 1);
            Triangles.Emplace(iVertexX + VertexCountInLine * (iVertexY + 1));

            Triangles.Emplace(iVertexX + VertexCountInLine * iVertexY + 1);
            Triangles.Emplace(iVertexX + VertexCountInLine * (iVertexY + 1) + 1);
            Triangles.Emplace(iVertexX + VertexCountInLine * (iVertexY + 1));
        }
    }

    PlaneTile->SetData(Vertices, UVs, Triangles, Normals, Tangents);

    // ищем пустой индекс сетки, пока не ушли в игровой поток
    int32 MeshSectionIndex = 0;
    while (ContainsPlaneByMeshSectionIndex(MeshSectionIndex)) ++MeshSectionIndex;
    PlaneTile->SetMeshSectionIndex(MeshSectionIndex);

    PlaneTile->SetState(EDMPlaneTileState::CalculatedPlane);
}

void ADMLandscapeGenerator::CalculateFoliageAsync()
{
    FDMPlaneTile* PlaneTile = FindPlaneByState(EDMPlaneTileState::SpawnedPlane);
    if (!PlaneTile) return;

    PlaneTile->SetState(EDMPlaneTileState::CalculateFoliage);

    const FIntVector2 Start = FIntVector2(PlaneTile->GetTile().X * TileLen, PlaneTile->GetTile().Y * TileLen);
    const FIntVector2 End =
        FIntVector2(PlaneTile->GetTile().X * TileLen + TileLen, PlaneTile->GetTile().Y * TileLen + TileLen);

    for (FDMISM& Foliage : Foliages)
    {
        if (!Foliage.bEnable) continue;

        const uint16 MaxCount = (Foliage.MaxCount * TileLen) / LandscapeGenerator::DefaultMeasurementsSize;
        for (int16 iStep = 0; iStep < MaxCount; ++iStep)
        {
            CalculateInstancesTransform(Foliage, Start, End);
        }
    }

    PlaneTile->SetState(EDMPlaneTileState::CalculatedFoliage);
}

void ADMLandscapeGenerator::CalculateGrassAsync()
{
    const uint8 GrassRadius = (GrassCount - 1) / 2;

    for (FDMISMWithTiles& Grass : Grasses)
    {
        if (!Grass.bEnable) continue;

        // удаляем дальние плитки из истории
        for (int16 iTile = Grass.Tiles.Num() - 1; iTile >= 0; --iTile)
        {
            const FIntVector2 GrassTile = Grass.Tiles[iTile];
            if (IsExternalTile(GrassTile, CurrentGrassTile, GrassRadius))
            {
                Grass.Tiles.RemoveAt(iTile);
            }
        }

        // рассчитываем трансформацию инстансов для новых плиток
        for (int8 iX = -GrassRadius; iX <= GrassRadius; ++iX)
        {
            for (int8 iY = -GrassRadius; iY <= GrassRadius; ++iY)
            {
                const FIntVector2 Tile = FIntVector2(CurrentGrassTile.X + iX, CurrentGrassTile.Y + iY);
                if (Grass.Tiles.Contains(Tile)) continue;

                const FIntVector2 Start = FIntVector2(Tile.X * GrassLen, Tile.Y * GrassLen);
                const FIntVector2 End = FIntVector2(Tile.X * GrassLen + GrassLen, Tile.Y * GrassLen + GrassLen);

                const uint16 MaxCount = (Grass.MaxCount * GrassLen) / LandscapeGenerator::DefaultMeasurementsSize;
                for (int16 iStep = 0; iStep < MaxCount; ++iStep)
                {
                    // если расчет удачный, добавляем запись в историю
                    if (CalculateInstancesTransform(Grass, Start, End))
                    {
                        Grass.Tiles.Emplace(Tile);
                    }
                }
            }
        }
    }

    ExecutionState = EDMExecutionState::CalculatedGrass;
}

float ADMLandscapeGenerator::CalculateZ(const FVector2D Location) const
{
    float Z = 0.f;
    for (const FDMPerlinSetting& NoiseSetting : NoiseSettings)
    {
        const FVector2D Value = RandomNoiseOffset + Location * NoiseSetting.Frequency + NoiseSetting.Offset;
        Z += FMath::PerlinNoise2D(Value) * NoiseSetting.Amplitude;
    }
    return Z;
}

bool ADMLandscapeGenerator::CalculateInstancesTransform(FDMISM& ISM, const FIntVector2 Start, const FIntVector2 End)
{
    const int32 RandomX = FMath::RandRange(Start.X, End.X);
    const int32 RandomY = FMath::RandRange(Start.Y, End.Y);

    FHitResult HitResults;
    FCollisionQueryParams CollisionParams;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResults,
        FVector(RandomX, RandomY, LandscapeGenerator::LineTraceLength),  // верх
        FVector(RandomX, RandomY, -LandscapeGenerator::LineTraceLength), // низ
        ECC_Visibility, CollisionParams);

    if (!bHit) return false;
    if (HitResults.Location.Z < ISM.MinHeight || HitResults.Location.Z > ISM.MaxHeight) return false;
    if (HitResults.Component != ProceduralMeshComponent) return false;

    const float DotProduct = FVector::DotProduct(HitResults.ImpactNormal, FVector::UpVector);
    const float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
    if (SlopeAngle < ISM.MinSlopeAngle || SlopeAngle > ISM.MaxSlopeAngle) return false;

    // TODO: (1) [КРИТИЧНО] Transform рассчитывается криво, из-за этого иногда валятся ошибки:
    // Re-target instance indices for shifting of array - Проблемы со Scale? Нулевой? Иногда размер с планету
    // Make sure Rotation is normalized when we turn it into a matrix. - Проблема с Rotation?
    FTransform InstanceTransform = FTransform();
    InstanceTransform.SetLocation(HitResults.Location);
    InstanceTransform.SetScale3D(FVector::One() * FMath::FRandRange(ISM.MinScale, ISM.MaxScale));

    const float InstancePitch = FMath::FRandRange(0.f, ISM.Rotation.Pitch);
    const float InstanceYaw = FMath::FRandRange(0.f, ISM.Rotation.Yaw);
    const float InstanceRoll = FMath::FRandRange(0.f, ISM.Rotation.Roll);
    InstanceTransform.SetRotation(FRotator(InstancePitch, InstanceYaw, InstanceRoll).Quaternion());

    // записываем данные в очередь
    ISM.AddTransform(InstanceTransform);

    return true;
}

void ADMLandscapeGenerator::StartAsyncTask(const EDMAsyncTaskType TaskType)
{
    if (bAsyncTaskIsBusy) return;

    bAsyncTaskIsBusy = true;
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
        [&, TaskType]()
        {
            FAsyncTask<FDMCalculationAsync>* CalculationTask = new FAsyncTask<FDMCalculationAsync>(this, TaskType);
            CalculationTask->StartBackgroundTask();
            CalculationTask->EnsureCompletion();
            delete CalculationTask;
        });
}

void ADMLandscapeGenerator::StartTimer()
{
    if (!GetWorld()->GetTimerManager().IsTimerActive(UpdateTimerHandle))
    {
        GetWorld()->GetTimerManager().SetTimer(
            UpdateTimerHandle, this, &ADMLandscapeGenerator::UpdateByTimer, TimerRate, true);
    }
}

void ADMLandscapeGenerator::StopTimer()
{
    if (bUpdatePlaniageRequest || bUpdateGrassRequest) return;
    GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);
}

bool ADMLandscapeGenerator::ContainsPlaneByState(const EDMPlaneTileState State) const
{
    return PlaneTiles.ContainsByPredicate(
        [State](const FDMPlaneTile& PlaneTile) { return PlaneTile.GetState() == State; });
}

bool ADMLandscapeGenerator::ContainsPlaneByMeshSectionIndex(const int32 MeshSectionIndex) const
{
    return PlaneTiles.ContainsByPredicate([MeshSectionIndex](const FDMPlaneTile& PlaneTile)
        { return PlaneTile.GetMeshSectionIndex() == MeshSectionIndex; });
}

FDMPlaneTile* ADMLandscapeGenerator::FindPlaneByState(const EDMPlaneTileState State)
{
    return PlaneTiles.FindByPredicate([State](const FDMPlaneTile& PlaneTile) { return PlaneTile.GetState() == State; });
}

FDMPlaneTile* ADMLandscapeGenerator::FindPlaneByTile(const FIntVector2 Tile)
{
    return PlaneTiles.FindByPredicate([Tile](const FDMPlaneTile& PlaneTile) { return PlaneTile.GetTile() == Tile; });
}

// TODO: стоит принимать массив TArray<FDMISM>, чтобы расчеты не повторялись при нескольких типах меша
void PickupFreeInstances(FDMISM& InISM, const FIntVector2 CurrentTile, const FIntVector2 PrevTile,
    const uint8 SaveRadius, const uint16 TileLen)
{
    // подбираем освободившиеся инстансы
    // строим большую коробку в той области, от которой отдалился игрок

    const int8 StepX = CurrentTile.X - PrevTile.X;
    const int8 StepY = CurrentTile.Y - PrevTile.Y;

    if (StepX != 0 || StepY != 0)
    {
        const int32 RadiusLen = SaveRadius * TileLen;
        const int32 BoxLen = LandscapeGenerator::InstancesOverlappingBoxScale * TileLen;
        const int32 CurrentLocationX = CurrentTile.X * TileLen;
        const int32 CurrentLocationY = CurrentTile.Y * TileLen;

        // TODO: дублирование кода
        if (StepX != 0)
        {
            const int32 StartX = CurrentLocationX + (StepX > 0 ? -RadiusLen : RadiusLen + TileLen);
            const int32 EndX = CurrentLocationX + (StepX > 0 ? RadiusLen - BoxLen : RadiusLen + TileLen + BoxLen);
            const int32 StartY = CurrentLocationY - BoxLen;
            const int32 EndY = CurrentLocationY + BoxLen;
            const FBox Box = FBox(FVector(StartX, StartY, -BoxLen), FVector(EndX, EndY, BoxLen));
            TArray<int32> Instances = InISM.GetComponent()->GetInstancesOverlappingBox(Box);
            for (const int32 Instance : Instances) InISM.AddIndex(Instance);
        }

        if (StepY != 0)
        {
            const int32 StartX = CurrentLocationX - BoxLen;
            const int32 EndX = CurrentLocationX + BoxLen;
            const int32 StartY = CurrentLocationY + (StepY > 0 ? -RadiusLen : RadiusLen + TileLen);
            const int32 EndY = CurrentLocationY + (StepY > 0 ? RadiusLen - BoxLen : RadiusLen + TileLen + BoxLen);
            const FBox Box = FBox(FVector(StartX, StartY, -BoxLen), FVector(EndX, EndY, BoxLen));
            TArray<int32> Instances = InISM.GetComponent()->GetInstancesOverlappingBox(Box);
            for (const int32 Instance : Instances) InISM.AddIndex(Instance);
        }
    }
}

FIntVector2 GetTileByLocation(const FVector Location, const uint16 TileLen)
{
    const int32 IndexX = Location.X / TileLen + (Location.X >= 0 ? 0 : -1);
    const int32 IndexY = Location.Y / TileLen + (Location.Y >= 0 ? 0 : -1);
    return FIntVector2(IndexX, IndexY);
}

bool IsExternalTile(const FIntVector2 CheckedTile, const FIntVector2 CenterTile, const uint8 SaveRadius)
{
    return (CenterTile.X - SaveRadius) > CheckedTile.X || CheckedTile.X > (CenterTile.X + SaveRadius) ||
           (CenterTile.Y - SaveRadius) > CheckedTile.Y || CheckedTile.Y > (CenterTile.Y + SaveRadius);
}

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../LumberGameMode.h"
#include "ProceduralMeshComponent.h"
#include "Terrain.generated.h"

#define MAX_CHUNKS 5000

class UProceduralMeshComponent;
class UTreeLoader;
class ALumberGameMode;

UENUM()
enum EChunkRenderState {
	NotRendered,
	Rendering,
	Rendered
};

UENUM()
enum EChunkQuality {
	Low,
	Medium,
	High,
	Collision
};


USTRUCT()
struct FChunkRenderData {
	GENERATED_BODY()
	
	EChunkRenderState TerrainRenderState = EChunkRenderState::NotRendered;
	EChunkRenderState TreeRenderState = EChunkRenderState::NotRendered;
	EChunkRenderState BuildingsRenderState = EChunkRenderState::NotRendered;


	FVector2D ChunkLocation;

	EChunkQuality ChunkQuality;
	
	int ChunkIndex = -1;

};

USTRUCT()
struct FMeshData {
	GENERATED_BODY()
	TArray<FVector> Vertices;
	TArray<int> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> Colors;
};

UENUM()
enum EJobStatus {
	NotRunning,
	Running,
	Completed
};

USTRUCT()
struct FJobBatch {
	GENERATED_BODY()
	TArray<TFunction<void()>> Jobs;
	EJobStatus JobStatus = EJobStatus::NotRunning;
};

UCLASS()
class LUMBER_API ATerrain : public AActor
{
	GENERATED_BODY()

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void GenerateMesh(TArray<FVector> Vertices, TArray<int> Triangles, TArray<FVector> Normals, TArray<FVector2D> UVs, TArray<FProcMeshTangent> Tangents, TArray<FColor> Colors);


	int ExtractRandomNumber(int* i_Seed);

	void ResetLookupTable();

public:	
	// Sets default values for this actor's properties
	ATerrain();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	float GetNoiseValueAtPoint(FVector2D Point, float Frequency, int* i_Seed);

	void RenderChunks(FVector2D From);

	void QueueChunkLoad(FVector2D ChunkLocation);

	void ReloadChunk(FVector2D ChunkLocation);

	void ReloadChunk(int ChunkIndex);

	bool CheckChunkForReloading(FVector2D ChunkLocation);

	bool CheckChunkForReloading(int ChunkIndex);

	EChunkQuality GetTargetLODForChunk(FVector2D ChunkLocation);

	void AddJobs(const TArray<TFunction<void()>>& Jobs);

	void AddJob(TFunction<void()> Job);

	void RunJobs();

	void RunJob(TFunction<void()> Job);

	void RunBatchJob(const TArray<TFunction<void()>>& Jobs);

	void DeleteChunkAtIndex(int ChunkIndex);

	void LoadChunk(int ChunkDataIndex);


	void GetNearestChunks(TArray<FVector2D>* NearestChunks);

	FVector2D GetClosestChunkToPoint(FVector2D Point);

	void RecursiveRender(FVector2D ChunkCoord, int Iteration);

	int DesignateChunkIndex(FVector2D ChunkLocation, EChunkQuality ChunkQuality);

	int FindOrCreateChunkData(FVector2D ChunkLocation, EChunkQuality ChunkQuality);

	void GetChunkRenderData(FMeshData* MeshData, FVector2D ChunkCoord, EChunkQuality Quality);

	int GetChunk(FVector2D ChunkLocation);

	void GetChunkSizesFromQuality(EChunkQuality Quality, int* NewChunkSize, int* NewTileSize);

	void CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);

	void CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);

	int AddNewChunkData(FChunkRenderData NewData);

	bool ChunkValid(int ChunkIndex);

	float GetTerrainPointData(FVector2D Point);

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UProceduralMeshComponent* Mesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UProceduralMeshComponent* CollisionMesh;

	ALumberGameMode* Gamemode;

public:
	// Length and width of each chunk, must be odd and greater than 3
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int chunkSize = 100;

	// Length and width of each tile, which consists of two triangles
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int tileSize = 600;

	// (Chunksize - 1) x tileSize
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int totalChunkSize;

	// Terrain material
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMaterialInstance* TerrainMaterial;

	// Chunk render distance, eg Distance=3 means 3 chunks from observer will be rendered
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int ChunkRenderDistance = 5;

	// Distance from render distance in chunks at which chunks will start to be unloaded, must be higher than chunk render distance
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int ChunkDeletionOffset = 5;

	// Which intervals should rendercheck be called? In seconds
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float RenderCheckPeriod = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bMakeCollision = false;

	// Distance at which point onwards the terrain is medium quality in chunks
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int MediumLODCutoffDist = 2;

	// Distance at which point onwards the terrain is low quality
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int LowLODCutoffDist = 3;

	// How many render jobs should be run in order at once, less jobs = faster computation & more lag, more jobs = slower computation & less lag
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int JobsPerBatch = 10;

	// Local Variables
public:
	// Generates chunks from this point
	AActor *ActorToGenerateFrom;

	// Generates chunks from this point
	FVector2D ObserverLocation;

	// Stream for psudorandom numbers
	FRandomStream Stream;

	// An array of integers with pseudorandom numbers that will always be the same for the same seed, kind of like a look-up table
	TArray<int> SeedArray = TArray<int>();

	// Stores chunks as pairs of their location (for unrendering) and their index, which correspond to the ProceduralMeshComponent's Mesh Section Index
	TArray<FChunkRenderData> Chunks;

	// Stores arrays of array of jobs, which gets processed every tick
	TArray<FJobBatch> JobBatches;

public:
	float NextChunkRenderCheck = 2;
	bool bCheckingRender = false;

	float NextChunkDeletionCheck = 2;

private:
	void LoadChunkTerrain(int ChunkDataIndex, EChunkQuality ChunkTargetQuality, FVector2D ChunkCoord);

	void LoadChunkTrees(int ChunkDataIndex, EChunkQuality ChunkTargetQuality, FVector2D ChunkCoord);

	void LoadChunkBuildings();

public:
	/*
		Called by TreeLoader once a chunk has finished generating its trees
	*/
	void OnFinishLoadedChunkTrees(int ChunkDataIndex);
};

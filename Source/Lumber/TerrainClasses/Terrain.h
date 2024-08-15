// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Terrain.generated.h"

class UProceduralMeshComponent;

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
	EChunkRenderState RenderState;
	FVector2D Coordinates;

	EChunkQuality ChunkQuality;
	int ChunkIndex;
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

UCLASS()
class LUMBER_API ATerrain : public AActor
{
	GENERATED_BODY()
	

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void GenerateMesh(TArray<FVector> Vertices, TArray<int> Triangles, TArray<FVector> Normals, TArray<FVector2D> UVs, TArray<FProcMeshTangent> Tangents, TArray<FColor> Colors);

	float GetTerrainPointData(FVector2D Point);

	int ExtractRandomNumber(int* i_Seed);

	void ResetLookupTable();

public:	
	// Sets default values for this actor's properties
	ATerrain();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	float GetNoiseValueAtPoint(FVector2D Point, float Frequency, int* i_Seed);

	void RenderChunks(FVector2D From);

	void RunBatchJob(const TArray<TFunction<void()>>& Jobs);

	void RenderSingleChunk(FChunkRenderData ChunkData);

	void RemoveChunk(FVector2D ChunkLocationToRemove);

	void RemoveChunk(FChunkRenderData* ChunkToRemove);

	void GetNearestChunks(TArray<FVector2D>* NearestChunks);

	FVector2D GetClosestChunkToPoint(FVector2D Point);

	void RecursiveRender(FVector2D ChunkCoord, int Iteration);

	void UpdateChunk(FVector2D ChunkCoord, EChunkQuality Quality);

	FChunkRenderData* DesignateChunkIndex(FVector2D ChunkLocation, EChunkQuality ChunkQuality);

	FChunkRenderData* FindOrCreateChunkData(FVector2D ChunkLocation, EChunkQuality ChunkQuality);

	void GetChunkRenderData(FMeshData* MeshData, FVector2D ChunkCoord, EChunkQuality Quality);

	FChunkRenderData* GetChunk(FVector2D ChunkLocation);

	int CheckChunk(FVector2D ChunkLocation, FChunkRenderData** FoundRenderData);

	void GetChunkSizesFromQuality(EChunkQuality Quality, int* NewChunkSize, int* NewTileSize);

	void CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);

	void CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UProceduralMeshComponent* Mesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UProceduralMeshComponent* CollisionMesh;

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

	// Stream for psudorandom numbers
	FRandomStream Stream;

	// An array of integers with pseudorandom numbers that will always be the same for the same seed, kind of like a look-up table
	TArray<int> SeedArray = TArray<int>();

	// Stores chunks as pairs of their location (for unrendering) and their index, which correspond to the ProceduralMeshComponent's Mesh Section Index
	TArray<FChunkRenderData> Chunks;

public:
	float NextChunkRenderCheck = 2;
	bool bCheckingRender = false;
};

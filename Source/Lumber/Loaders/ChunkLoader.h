// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../Serialization/MyWorld.h"
#include "GameFramework/Actor.h"
#include "Loader.h"
#include "../LumberGameMode.h"
#include "ProceduralMeshComponent.h"
#include "ChunkLoader.generated.h"

#define MAX_CHUNKS 5000

class UProceduralMeshComponent;
class ATreeLoader;
class ATerrain;
class AJobHandler;

UCLASS()
class LUMBER_API AChunkLoader : public ALoader
{
GENERATED_BODY()

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

private:
	// Generates chunks from this point
	AActor* ActorToGenerateFrom;

	// Generates chunks from this point
	FVector2D ObserverLocation;

	// Stores chunks as pairs of their location (for unrendering) and their index, which correspond to the ProceduralMeshComponent's Mesh Section Index
	TArray<FChunkRenderData> Chunks;

	//Debug switches to turn on or off features
	bool bDebugGenerateTrees = false;
	bool bDebugGenerateTerrain = true;

public:

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay();

	void RenderChunks(FVector2D From);

	void QueueChunkLoad(FVector2D ChunkLocation);

	void ReloadChunk(FVector2D ChunkLocation);

	void ReloadChunk(int ChunkIndex);

	bool CheckChunkForReloading(FVector2D ChunkLocation);

	bool CheckChunkForReloading(int ChunkIndex);

	EChunkQuality GetTargetLODForChunk(FVector2D ChunkLocation);

	void DeleteChunkAtIndex(int ChunkIndex);

	void LoadChunk(int ChunkDataIndex);



	void GetNearestChunks(TArray<FVector2D>* NearestChunks);

	FVector2D GetClosestChunkToPoint(FVector2D Point);

	void RecursiveRender(FVector2D ChunkCoord, int Iteration);

	int DesignateChunkIndex(FVector2D ChunkLocation, EChunkQuality ChunkQuality);

	int FindOrCreateChunkData(FVector2D ChunkLocation, EChunkQuality ChunkQuality);


	int GetChunk(FVector2D ChunkLocation);

	void GetChunkSizesFromQuality(EChunkQuality Quality, int* NewChunkSize, int* NewTileSize);

	int AddNewChunkData(FChunkRenderData NewData);

	bool ChunkValid(int ChunkIndex);

	// Local Variables
public:



public:
	float NextChunkRenderCheck = 2;
	bool bCheckingRender = false;

	float NextChunkDeletionCheck = 2;

	UMyWorld *LoadedWorld;
private:

	void LoadChunkTrees(int ChunkDataIndex, EChunkQuality ChunkTargetQuality, FVector2D ChunkCoord);

	void LoadChunkBuildings();

public:
	/*
		Called by TreeLoader once a chunk has finished generating its trees
	*/
	void OnFinishLoadedChunkTrees(int ChunkDataIndex);

public:
	AChunkLoader();

	TSharedPtr<FJsonObject> static SerializeWorldSettings(FMyWorldData WorldData);

public:
	void LoadFile(FString FileAddress);
	void SaveFile(FString FileAddress);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/NoExportTypes.h"
#include "../LumberGameMode.h"
#include "Loader.generated.h"

#define GamePriority ENamedThreads::GameThread 
#define BackgroundPriority ENamedThreads::AnyBackgroundHiPriTask

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

class ALumberGameMode;
/**
 * 
 */
UCLASS()
class LUMBER_API ALoader : public AActor
{
	GENERATED_BODY()
	
public:
	ALoader();

public:
	/*
		Sets gamemode reference
	*/
	void SetGamemode(ALumberGameMode* NewGamemode);

	/*
		Called by Chunkloader for this loader to start generating stuff at that chunk
	*/
	virtual void StartGeneration(int ChunkDataIndex, FVector2D ChunkCoord, EChunkQuality Quality);

	/*
		Should be called once by the final step of this loader, to notify chunkloader that this part of the chunk is loaded.
	*/
	virtual void FinishGeneration(int ChunkDataIndex);

public:
	ALumberGameMode* Gamemode;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Loader.generated.h"

class ALumberGamemode;
enum EChunkQuality;
/**
 * 
 */
UCLASS()
class LUMBER_API ULoader : public UObject
{
	GENERATED_BODY()
	
public:
	/*
		Sets gamemode reference
	*/
	void SetGamemode(ALumberGamemode * NewGamemode);

	/*
		Called by Chunkloader for this loader to start generating stuff at that chunk
	*/
	virtual void StartGeneration(int ChunkDataIndex, FVector2D ChunkCoord, EChunkQuality Quality);

	/*
		Should be called once by the final step of this loader, to notify chunkloader that this part of the chunk is loaded.
	*/
	virtual void FinishGeneration(int ChunkDataIndex);

protected:
	ALumberGamemode* Gamemode;
};

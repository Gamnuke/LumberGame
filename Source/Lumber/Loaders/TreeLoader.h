// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TreeLoader.generated.h"

class ALumberGameMode;

struct FTreeChunkRenderData {
	int ChunkIndex;
	TArray<bool*> AssignedTrees;
};

/**
 * Class that ideally works with the Terrain class to efficiently load Trees, using call back functions to notify end of generation
 */
UCLASS()
class LUMBER_API UTreeLoader : public UObject
{
	GENERATED_BODY()

public:
	ALumberGameMode* Gamemode;

public:
	UTreeLoader();

	/*
		Container to hold ongoing tree generation tasks. Outer array resembles each chunk, contaning array of bools.
		Inner array resembles each tree for that chunk.
	*/
	TArray<FTreeChunkRenderData*> TreeCompletion;

	/*
		Called by the tree once it has fully completed its generation
	*/
	void OnGeneratedTree(FTreeChunkRenderData* AssignedArray, bool *AssignedTree);

	void GenerateTrees(int ChunkDataIndex, FVector2D ChunkCoord);

	/*
		Returns if all trees in array are rendered (all bools are true)
	*/
	bool TreesInChunkRendered(TArray<bool*> Array);

	/*
		Returns this treeloader
	*/
	static UTreeLoader* GetTreeLoader();
};

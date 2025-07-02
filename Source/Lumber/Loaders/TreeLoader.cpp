// Fill out your copyright notice in the Description page of Project Settings.


#include "TreeLoader.h"
#include "Terrain.h"
#include "../TreeClasses/TreeRoot.h"
#include "../LumberGameMode.h"
#include "Kismet/GameplayStatics.h"

UTreeLoader::UTreeLoader()
{
}

void UTreeLoader::OnGeneratedTree(FTreeChunkRenderData *AssignedArray, bool* AssignedTree)
{
	// Tree completed rendering, assign it to true in TreeCompletion array
	*AssignedTree = true;

	// If all trees in the chunk are completed, remove it from the array and notify Terrain this chunk has completed tree render
	if (TreesInChunkRendered(AssignedArray->AssignedTrees)) {

		// Notify chunkloader
		Gamemode->GetTerrain()->OnFinishLoadedChunkTrees(AssignedArray->ChunkIndex);
		UE_LOG(LogTemp, Warning, TEXT("Finished Generating Trees for Chunk"));
		TreeCompletion.Remove(AssignedArray);
	}
}

void UTreeLoader::GenerateTrees(int ChunkDataIndex, FVector2D ChunkCoord)
{
	AsyncTask(ENamedThreads::GameThread, [this, ChunkDataIndex, ChunkCoord] {
	ATerrain* Terrain = Gamemode->GetTerrain();

		int NewChunkSize = Terrain->chunkSize;
		int NewTileSize = Terrain->tileSize;

		int scale = 10;
		NewChunkSize /= scale;
		NewTileSize *= scale;

		// Create new TreeChunkRenderData to keep track of Trees and their associated chunk to track generation progress
		FTreeChunkRenderData NewTreeChunkRenderData = FTreeChunkRenderData();
		NewTreeChunkRenderData.ChunkIndex = ChunkDataIndex;

		TreeCompletion.Add(&NewTreeChunkRenderData);

		for (int row_i = 0; row_i < NewChunkSize + 1; row_i++) {
			for (int col_i = 0; col_i < NewChunkSize + 1; col_i++) {
				bool NewState = false;
				NewTreeChunkRenderData.AssignedTrees.Add(&NewState);

				FVector2D Point = ChunkCoord + FVector2D(NewTileSize * row_i, NewTileSize * col_i);
				FVector NewVertex = FVector(Point.X, Point.Y, Terrain->GetTerrainPointData(Point));

				if (Gamemode->TreeRootBlueprintClass != nullptr) {
					ATreeRoot* NewTree = GetWorld()->SpawnActor<ATreeRoot>(Gamemode->TreeRootBlueprintClass, NewVertex, FRotator());
					NewTree->TreeSeed = FMath::Rand();
					NewTree->GenerateTree(EChunkQuality::Low, &NewTreeChunkRenderData, &NewState);
				}
				else {
					UE_LOG(LogTemp, Warning, TEXT("NO TREE ROOT BLUEPRINT"));
				}
			}
		}
	});
}

bool UTreeLoader::TreesInChunkRendered(TArray<bool*> Array)
{
	for (int i = 0; i < Array.Num(); i++)
	{
		bool element = *Array[i];
		if (element == false) {
			return false;
		}
	}

	return true;
}

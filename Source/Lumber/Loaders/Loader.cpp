// Fill out your copyright notice in the Description page of Project Settings.


#include "Loader.h"

void ULoader::SetGamemode(ALumberGamemode* NewGamemode)
{
	Gamemode = NewGamemode;
}

void ULoader::StartGeneration(int ChunkDataIndex, FVector2D ChunkCoord, EChunkQuality Quality)
{
}

void ULoader::FinishGeneration(int ChunkDataIndex)
{
}

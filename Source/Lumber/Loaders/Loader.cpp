// Fill out your copyright notice in the Description page of Project Settings.


#include "Loader.h"

ALoader::ALoader()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ALoader::SetGamemode(ALumberGameMode* NewGamemode)
{
	Gamemode = NewGamemode;
}

void ALoader::StartGeneration(int ChunkDataIndex, FVector2D ChunkCoord, EChunkQuality Quality)
{
}

void ALoader::FinishGeneration(int ChunkDataIndex)
{
}

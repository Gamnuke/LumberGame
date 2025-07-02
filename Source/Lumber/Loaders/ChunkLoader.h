// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../Serialization/MyWorld.h"
#include "ChunkLoader.generated.h"


UCLASS()
class LUMBER_API UChunkLoader : public UObject
{
GENERATED_BODY()

public:
	UChunkLoader();

	void static CreateNewWorld(FString WorldName, int Seed = 0);

	TSharedPtr<FJsonObject> static SerializeWorldSettings(FMyWorldSettings WorldData);

public:
	void LoadFile(FString FileAddress);
	void SaveFile(FString FileAddress);
};

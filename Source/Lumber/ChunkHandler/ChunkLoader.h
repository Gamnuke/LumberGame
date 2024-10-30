// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MyWorld.h"
//#include "ChunkLoader.generated.h"



class LUMBER_API ChunkLoader
{
public:
	ChunkLoader();

	void static CreateNewWorld(FString WorldName, int Seed = 0);

	TSharedPtr<FJsonObject> static SerializeWorldSettings(FMyWorldSettings WorldData);

public:
	void LoadFile(FString FileAddress);
	void SaveFile(FString FileAddress);
};

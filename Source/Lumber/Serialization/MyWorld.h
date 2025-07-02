// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SerializableObject.h"
#include "MyWorld.generated.h"

USTRUCT()
struct FMyWorldSettings {
	GENERATED_BODY()
	int Seed;
	FString WorldName;
};

class LUMBER_API MyWorld : public ISerializableObject
{
public:
	MyWorld();

	FMyWorldSettings WorldData;

	FJsonObject SerializeObject();
	void DeserializeAndLoadObject(FJsonObject ObjectData);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SerializableObject.h"
#include "MyWorld.generated.h"


/*
	Used to define how the layer of noise behaves in one set of noise
*/
UENUM(BlueprintType)
enum EOperationType {
	Additive,
	Multiplicative
};

/*
	Represents one layer of noise, which can be scaled and specified to be added or multiplicated
*/
USTRUCT(BlueprintType)
struct FNoiseLayer {
	GENERATED_BODY()

	// Scale of the layer (how stretched the mask is)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float XScale = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float YScale = 1;

	// Shifts the mask by some amount
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float XOffset = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float YOffset = 0;

	// How much gain this layer has
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Gain = 1;
	
	// Whether this mask should be added, multiplicated with the result, etc..
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TEnumAsByte<EOperationType> OperationType;
};

USTRUCT(BlueprintType)
struct FMyWorldData {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int Seed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString WorldName;
};

USTRUCT(BlueprintType)
struct FMyWorldSettings {
	GENERATED_BODY()

	// Noise procedure settings for mountains, we dont need to save or load this so we can just leave it as a default object
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FNoiseLayer> MountainLayer;

};

UCLASS(Blueprintable)
class LUMBER_API UMyWorld : public UObject, public ISerializableObject
{
	GENERATED_BODY()

public:
	UMyWorld();

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FMyWorldData WorldData;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FMyWorldSettings WorldSettings;

public:

	TSharedPtr<FJsonObject> SerializeObject();
	void DeserializeAndLoadObject(TSharedPtr<FJsonObject> ObjectData);

	// Creates new world and returns MyWorld object
	static UMyWorld* CreateNewWorld(UObject* Owner);

	// Creates new world given an optional blueprint world class
	static UMyWorld* CreateNewWorld(UObject* Owner, TSubclassOf<UMyWorld> Class);

	// Loads world from file
	static UMyWorld* LoadWorldFromFile(FString WorldName);

	// Saves world to file
	static void SaveWorldToFile(UMyWorld* WorldToSave);

};

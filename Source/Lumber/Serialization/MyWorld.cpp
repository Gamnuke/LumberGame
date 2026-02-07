// Fill out your copyright notice in the Description page of Project Settings.


#include "MyWorld.h"
#include "FileHelpers.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerMacros.h"

UMyWorld::UMyWorld()
{
}

// Serialize world into json file
TSharedPtr<FJsonObject> UMyWorld::SerializeObject()
{
	TSharedPtr<FJsonObject> newJsonObject = MakeShareable(new FJsonObject());
	TSharedPtr<FJsonObject> NewWorldData = MakeShareable(new FJsonObject());

	NewWorldData->SetStringField("WorldName", WorldData.WorldName);
	NewWorldData->SetNumberField("Seed", WorldData.Seed);

	// Final step
	newJsonObject->SetObjectField("WorldData", NewWorldData);
	return newJsonObject;
}

// Load the world
void UMyWorld::DeserializeAndLoadObject(TSharedPtr<FJsonObject> ObjectData)
{
	TSharedPtr<FJsonObject> NewWorldData = ObjectData->GetObjectField("WorldData");

	WorldData.WorldName = NewWorldData->GetStringField("WorldName");
	WorldData.Seed = NewWorldData->GetNumberField("Seed");
}

UMyWorld* UMyWorld::CreateNewWorld(UObject *Owner)
{
	// Create new world and set random seed
	UMyWorld *NewWorld = NewObject<UMyWorld>(Owner, "NewWorld");
	NewWorld->WorldData.Seed = FMath::Rand();

	// Save world to file
	SaveWorldToFile(NewWorld);

	return NewWorld;
}

UMyWorld* UMyWorld::CreateNewWorld(UObject* Owner, TSubclassOf<UMyWorld> Class)
{
	// Create new world and set random seed
	UMyWorld* NewWorld = NewObject<UMyWorld>(Owner, Class, "NewWorld");
	NewWorld->WorldData.Seed = FMath::Rand();

	// Save world to file
	SaveWorldToFile(NewWorld);

	return NewWorld;
}

//TODO Implement
UMyWorld* UMyWorld::LoadWorldFromFile(FString WorldName)
{
	return nullptr;
}

void UMyWorld::SaveWorldToFile(UMyWorld* WorldToSave)
{
	// Create the JSON object
	TSharedPtr<FJsonObject> JsonObject = WorldToSave->SerializeObject();

	// Convert the JSON object to a string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	// Define the file path (in this case, under the project's Saved directory)
	FString FilePath = FPaths::ProjectDir() + "/Saved/" + WorldToSave->WorldData.WorldName + "/WorldSettings.json";

	// Save the string to a file
	if (FFileHelper::SaveStringToFile(OutputString, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("JSON saved successfully to %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save JSON to file."));
	}
}

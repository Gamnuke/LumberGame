// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkLoader.h"
#include "FileHelpers.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerMacros.h"

UChunkLoader::UChunkLoader()
{
}

void UChunkLoader::CreateNewWorld(FString WorldName, int Seed)
{
	if (Seed == 0) {
		Seed = FMath::Rand();
	}

    FMyWorldSettings newWorldSettings;
	newWorldSettings.Seed = Seed;
    newWorldSettings.WorldName = WorldName;

    // Create the JSON object
    TSharedPtr<FJsonObject> JsonObject = SerializeWorldSettings(newWorldSettings);

    // Convert the JSON object to a string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    // Define the file path (in this case, under the project's Saved directory)
    FString FilePath = FPaths::ProjectDir() + "/Saved/" + WorldName + "/WorldSettings.json";

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

TSharedPtr<FJsonObject> UChunkLoader::SerializeWorldSettings(FMyWorldSettings WorldData) {
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField("Seed", WorldData.Seed);
    JsonObject->SetStringField("WorldName", WorldData.WorldName);

    return JsonObject;
}

void UChunkLoader::LoadFile(FString FileAddress)
{
	//FFileHelper::LoadFileToArray()
}

void UChunkLoader::SaveFile(FString FileAddress)
{
	//FFileHelper::LoadFileToArray()
}

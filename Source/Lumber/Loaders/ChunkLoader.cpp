// Fill out your copyright notice in the Description page of Project Settings.


#include "ChunkLoader.h"
#include "FileHelpers.h"
#include "JsonUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerMacros.h"
#include "../LumberGameMode.h"
#include "JobHandler.h"
#include "TreeLoader.h"
#include "TerrainLoader.h"

AChunkLoader::AChunkLoader()
{
    // Construct mesh component

    totalChunkSize = (chunkSize)*tileSize;
	SetActorTickEnabled(true);
}


// Called when the game starts or when spawned
void AChunkLoader::BeginPlay()
{
    Super::BeginPlay();

    ActorToGenerateFrom = GetWorld()->GetFirstPlayerController()->GetPawn();
	if (ActorToGenerateFrom == nullptr) {
		GEngine->AddOnScreenDebugMessage(1, 1, FColor::Green, FString("Couldn't find observer actor"));
	}
	else {
		GEngine->AddOnScreenDebugMessage(1, 1, FColor::Green, FString("Found observer actor"));
	}
}

// Called every frame
void AChunkLoader::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (ActorToGenerateFrom != nullptr) {
        FVector Loc = ActorToGenerateFrom->GetActorLocation();
        ObserverLocation = FVector2D(Loc.X, Loc.Y) - FVector2D(totalChunkSize / 2, totalChunkSize / 2);
		GEngine->AddOnScreenDebugMessage(1, 1, FColor::Green, ObserverLocation.ToString());
    }
	else {
		GEngine->AddOnScreenDebugMessage(1, 1, FColor::Green, FString("Observer is null"));

	}

    // Check if its time to do a render check and if one isn't already running
    if (GetWorld()->TimeSeconds >= NextChunkRenderCheck && !bCheckingRender) {
        NextChunkRenderCheck = GetWorld()->TimeSeconds + RenderCheckPeriod;
        bCheckingRender = true;
        RenderChunks(ObserverLocation);
    }
}

void AChunkLoader::CreateNewWorld(FString WorldName, int Seed)
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

/*
Renders chunks around a given point on a seperate thread, based on render distance and set LOD distances.
Algorithm renders chunks in a grid, not from the point of the player
*/
void AChunkLoader::RenderChunks(FVector2D From) {
	AsyncTask(BackgroundPriority, [this, From]() {

		// Stores array of render jobs that need to be run
		//TArray<TFunction<void()>> Jobs;

		// Delete chunks that are outside render distance that are not still rendering
		for (int i = 0; i < Chunks.Num(); i++)
		{
			if (ChunkValid(i) && Chunks[i].TerrainRenderState == EChunkRenderState::Rendered && (From - Chunks[i].ChunkLocation).Length() > (ChunkRenderDistance + ChunkDeletionOffset) * totalChunkSize * sqrt(2)) {
				DeleteChunkAtIndex(i);
			}
		}

		// get array of chunk locations around player
		TArray<FVector2D> NearestChunks;
		GetNearestChunks(&NearestChunks);

		// Go through each chunk to check if it isnt already rendered, and render it, regardless of their LOD
		for (FVector2D ChunkToCheck : NearestChunks) {

			int FoundChunkIndex = GetChunk(ChunkToCheck);

			if (ChunkValid(FoundChunkIndex)) {
				if (CheckChunkForReloading(FoundChunkIndex)) {
					ReloadChunk(FoundChunkIndex);
				}
			}
			else {
				QueueChunkLoad(ChunkToCheck);
			}

			//// Get LOD of what this chunk should be
			//EChunkQuality TargetLOD = GetTargetLODForChunk(ChunkToCheck);
			//
			//// Check if chunk is found in memory, otherwise create new chunk data
			//int FoundChunkIndex = FindOrCreateChunkData(ChunkToCheck, TargetLOD);
			//FChunkRenderData FoundChunk = Chunks[FoundChunkIndex];

			//// Only start to render if either:
			//	// - The chunk is NOT rendered at all
			//	// - OR the chunk is not the right quality AND NOT already rendering
			//if (FoundChunk.RenderState == EChunkRenderState::NotRendered ||
			//	(FoundChunk.ChunkQuality != TargetLOD && FoundChunk.RenderState != EChunkRenderState::Rendering)
			//	) 
			//{
			//	// We have a new chunk or a chunk that needs to be updated
			//	FoundChunk.RenderState = EChunkRenderState::Rendering;
			//	FoundChunk.ChunkQuality = TargetLOD;
			//	Chunks[FoundChunkIndex] = FoundChunk;

			//	// Add rendering of this chunk to the job batch
			//	Jobs.Add([this, FoundChunkIndex]() { RenderChunkTerrain(FoundChunkIndex); });

		}

		Gamemode->GetJobHandler()->RunJobs();

		// finally end algorithm
		bCheckingRender = false;
	});
}

/*
* Allocates new chunk index, then sets it to rendering state, then queues a task to load the chunk.
* Initiates chunk load procedure if chunk is not already loaded, loading everything including terrain and other data like trees
*/
void AChunkLoader::QueueChunkLoad(FVector2D ChunkLocation) {

	// Chunk is already loaded, so don't do anything
	if (GetChunk(ChunkLocation) != -1) { return; }

	EChunkQuality NewChunkQuality = GetTargetLODForChunk(ChunkLocation);
	int NewChunkIndex = FindOrCreateChunkData(ChunkLocation, NewChunkQuality);
	Chunks[NewChunkIndex].TerrainRenderState = EChunkRenderState::Rendering;
	Chunks[NewChunkIndex].ChunkQuality = NewChunkQuality;

	Gamemode->GetJobHandler()->AddJob([this, NewChunkIndex]() {LoadChunk(NewChunkIndex); });
}

/*
Renders a single chunk given the index of the already created chunk data in the chunks array
*/
void AChunkLoader::LoadChunk(int ChunkDataIndex) {

	// Extract variables from ChunkData for easy access
	EChunkQuality ChunkTargetQuality = Chunks[ChunkDataIndex].ChunkQuality;
	FVector2D ChunkCoord = Chunks[ChunkDataIndex].ChunkLocation;

	// Here's where we would load other data, like terrain, generating trees, buildings, etc.

	// Start loading terrain for this chunk
	Gamemode->GetTerrainLoader()->LoadChunkTerrain(ChunkDataIndex, ChunkTargetQuality, ChunkCoord);

	// Start loading trees for this chunk
	Gamemode->GetTreeLoader()->GenerateTrees(ChunkDataIndex, ChunkCoord);

	// Set material on the game thread and set thte chunk data to be set as rendered
	AsyncTask(GamePriority, [this, ChunkDataIndex]() {
		Gamemode->GetTerrainLoader()->Mesh->SetMaterial(ChunkDataIndex, TerrainMaterial);
		Chunks[ChunkDataIndex].TerrainRenderState = EChunkRenderState::Rendered;
	});
}



void AChunkLoader::LoadChunkTrees(int ChunkDataIndex, EChunkQuality ChunkTargetQuality, FVector2D ChunkCoord) {
	Gamemode->GetTreeLoader()->GenerateTrees(ChunkDataIndex, ChunkCoord);
}

void AChunkLoader::LoadChunkBuildings() {

}

void AChunkLoader::OnFinishLoadedChunkTrees(int ChunkDataIndex)
{

}

/*
Deletes and reloades chunk
*/
void AChunkLoader::ReloadChunk(FVector2D ChunkLocation) {
	int ChunkIndex = GetChunk(ChunkLocation);
	if (ChunkIndex == -1) {
		QueueChunkLoad(ChunkLocation); 
		return;
	}

	ReloadChunk(ChunkIndex);
}

void AChunkLoader::ReloadChunk(int ChunkIndex) {
	if(ChunkValid(ChunkIndex)){
		EChunkQuality NewChunkQuality = GetTargetLODForChunk(Chunks[ChunkIndex].ChunkLocation);
		Chunks[ChunkIndex].TerrainRenderState = EChunkRenderState::Rendering;
		Chunks[ChunkIndex].ChunkQuality = NewChunkQuality;

		Gamemode->GetJobHandler()->AddJob([this, ChunkIndex]() {LoadChunk(ChunkIndex); });
	}
}

/*
Checks if a given chunk needs to be reloaded, to change LOD, if it isn't rendering
Returns false if chunk can't be found
*/
bool AChunkLoader::CheckChunkForReloading(int ChunkIndex) {
	// Check if chunk is valid
	if (ChunkIndex == -1) { return false; }

	// Check if chunk isn't rendering
	if (Chunks[ChunkIndex].TerrainRenderState == EChunkRenderState::Rendering) { return false; }

	// Check if chunk has the same LOD
	if (Chunks[ChunkIndex].ChunkQuality == GetTargetLODForChunk(Chunks[ChunkIndex].ChunkLocation)) { return false; }

	// Otherwise chunk is ready to be reloaded
	return true;
}

bool AChunkLoader::CheckChunkForReloading(FVector2D ChunkLocation) {
	return CheckChunkForReloading(GetChunk(ChunkLocation));
}

/*
Returns the LOD level from distance between player and the chunk
*/
EChunkQuality AChunkLoader::GetTargetLODForChunk(FVector2D ChunkLocation) {
	float ChunkDistance = (ChunkLocation - ObserverLocation).Length();

	if (ChunkDistance >= LowLODCutoffDist * totalChunkSize) {
		return EChunkQuality::Low;
	}
	else if (ChunkDistance >= MediumLODCutoffDist * totalChunkSize) {
		return EChunkQuality::Medium;

	}
	else if (ChunkDistance < MediumLODCutoffDist * totalChunkSize) {
		return EChunkQuality::High;
	}

	return EChunkQuality::High;
}



/*
Deletes a chunk given its index, by setting the ChunkIndex variable in the data at given index to -1, and
clearing the mesh section of that index.
*/
void AChunkLoader::DeleteChunkAtIndex(int ChunkIndex) {
	if (ChunkValid(ChunkIndex)) {
		AsyncTask(GamePriority, [this, ChunkIndex]() {
			Gamemode->GetTerrainLoader()->Mesh->ClearMeshSection(ChunkIndex);
			if (Chunks[ChunkIndex].ChunkQuality == EChunkQuality::High) {
				Gamemode->GetTerrainLoader()->CollisionMesh->ClearMeshSection(ChunkIndex);
			}
		});

		Chunks[ChunkIndex].ChunkIndex = -1;
	}
}






/*
Gets array of chunks within render distances of the player
*/
void AChunkLoader::GetNearestChunks(TArray<FVector2D> *NearestChunks) {

	// Round player location to nearest chunk
	FVector2D ClosestChunk = GetClosestChunkToPoint(ObserverLocation);

	for (int i = -ChunkRenderDistance; i < ChunkRenderDistance + 1; i++)
	{
		for (int j = -ChunkRenderDistance; j < ChunkRenderDistance + 1; j++)
		{
			NearestChunks->Add(ClosestChunk + FVector2D(i * totalChunkSize, j * totalChunkSize));
		}
	}
}

/*
returns the closest chunk coordinate to a point
*/
FVector2D AChunkLoader::GetClosestChunkToPoint(FVector2D Point) {
	int RoundedX = FMath::Floor(Point.X / (totalChunkSize)) * totalChunkSize;
	int RoundedY = FMath::Floor(Point.Y / (totalChunkSize)) * totalChunkSize;
	return FVector2D(RoundedX, RoundedY);
}



/*
Recursively renders adjacent chunks
*/
void AChunkLoader::RecursiveRender(FVector2D ChunkCoord, int Iteration) {
	//if (ChunkCoord.Length() >= totalChunkSize * 2) { return; }

	//// Check if this chunk is already rendered
	//for (int k = 0; k < Chunks.Num(); k++)
	//{
	//	// Check if we found this chunk in the chunk array
	//	if (Chunks.IsValidIndex(k) && ChunkCoord == Chunks[k].ChunkLocation)
	//	{
	//		// We found chunk in array, its either rendering or rendered, so move on
	//		return;
	//	}
	//}

	////RenderSingleChunk(ChunkCoord);

	//TArray<FVector2D> AdjacentChunks;

	//for (int i = -1; i <= 1; i++)
	//{
	//	for (int j = -1; j <= 1; j++)
	//	{
	//		FVector2D AdjacentChunk = GetClosestChunkToPoint(ChunkCoord + FVector2D(i * totalChunkSize, j * totalChunkSize));
	//		// Check if this chunk is already rendered

	//		RecursiveRender(AdjacentChunk, Iteration + 1);
	//	}
	//}
}


/*
Given a chunk location, create and return a new chunk data or return the existing data
*/
int AChunkLoader::FindOrCreateChunkData(FVector2D ChunkLocation, EChunkQuality ChunkQuality) {

	// check if chunk data exists
	int FoundChunkIndex = GetChunk(ChunkLocation);

	if (FoundChunkIndex == -1) {
		FoundChunkIndex = DesignateChunkIndex(ChunkLocation, ChunkQuality);
	}

	return FoundChunkIndex;
}

/*
Replaces the first old chunk data with an index of -1 with the new data, otherwise add to end of array of not found.
Returns index of the newly added data
*/
int AChunkLoader::AddNewChunkData(FChunkRenderData NewData) {

	// Go through array and replace the first chunk data with an index of -1, with the new data
	for (int i = 0; i < Chunks.Num(); i++)
	{
		if (Chunks.IsValidIndex(i) && Chunks[i].ChunkIndex == -1) {
			Chunks[i] = NewData;
			return i;
		}
	}

	// Otherwise we simply add to end of array
	return Chunks.Add(NewData);
}

/*
Checks if there is a valid chunk stored in the chunks array at given index
Must not be of chunk index -1 and should within array bounds
*/
bool AChunkLoader::ChunkValid(int ChunkIndex) {
	// Chunk index is out of bounds, so chunk is invalid
	if (!Chunks.IsValidIndex(ChunkIndex)) { return false; }

	// Chunk index is valid, but set 
	if (Chunks[ChunkIndex].ChunkIndex == -1) { return false; }
	return true;
}

/*
Returns the index of a chunk, given its location and sets struct pointer to that, returns -1 if its not found
*/
int AChunkLoader::GetChunk(FVector2D ChunkLocation) {
	for (int i = 0; i < Chunks.Num(); i++)
	{
		if (ChunkValid(i) && Chunks[i].ChunkLocation == ChunkLocation) {
			return i;
		}
	}

	return -1;
}


/*
Creates new chunk data at given location and LOD, and designates an index in the chunk array for this new chunk, 
and returns the index of the newly created chunk data
*/
int AChunkLoader::DesignateChunkIndex(FVector2D ChunkLocation, EChunkQuality ChunkQuality) {
	FChunkRenderData NewChunkData;

	// Add chunk data to array
	NewChunkData.ChunkLocation = ChunkLocation;
	NewChunkData.TerrainRenderState = EChunkRenderState::NotRendered;
	NewChunkData.ChunkQuality = ChunkQuality;
	int DesignatedIndex = AddNewChunkData(NewChunkData);
	Chunks[DesignatedIndex].ChunkIndex = DesignatedIndex;
	GEngine->AddOnScreenDebugMessage(FMath::Rand(), RenderCheckPeriod * 2, FColor::MakeRandomColor(), FString("Designated ") + ChunkLocation.ToString() + FString(" at ") + FString::FromInt(DesignatedIndex));

	return DesignatedIndex;
}


/*
Returns scaled chunk sizes and tile sizes with the LOD, should always try and equal to the initial totalChunkSize
*/
void AChunkLoader::GetChunkSizesFromQuality(EChunkQuality Quality, int *NewChunkSize, int *NewTileSize) {
	switch (Quality)
	{
	case Low:
		*NewChunkSize = chunkSize / 10;
		*NewTileSize = tileSize * 10;
		break;
	case Medium:
		*NewChunkSize = chunkSize / 5;
		*NewTileSize = tileSize * 5;
		break;
	case High:
		*NewChunkSize = chunkSize;
		*NewTileSize = tileSize;
		break;
	case Collision:
		*NewChunkSize = chunkSize;
		*NewTileSize = tileSize;
		break;
	default:
		break;
	}
}




TSharedPtr<FJsonObject> AChunkLoader::SerializeWorldSettings(FMyWorldSettings WorldData) {
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField("Seed", WorldData.Seed);
    JsonObject->SetStringField("WorldName", WorldData.WorldName);

    return JsonObject;
}

void AChunkLoader::LoadFile(FString FileAddress)
{
	//FFileHelper::LoadFileToArray()
}

void AChunkLoader::SaveFile(FString FileAddress)
{
	//FFileHelper::LoadFileToArray()
}

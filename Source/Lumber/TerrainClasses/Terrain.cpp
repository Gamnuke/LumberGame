// Fill out your copyright notice in the Description page of Project Settings.


#include "Terrain.h"
#include "KismetProceduralMeshLibrary.h"

// Sets default values
ATerrain::ATerrain()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Construct mesh component
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(FName("Mesh"));
	SetRootComponent(Mesh);

	totalChunkSize = (chunkSize) * tileSize;
}

// Called when the game starts or when spawned
void ATerrain::BeginPlay()
{
	Super::BeginPlay();
	ActorToGenerateFrom = GetWorld()->GetFirstPlayerController()->GetPawn();
	totalChunkSize = (chunkSize) * tileSize;

	// Set random seed for stream
	Stream.GenerateNewSeed();
	for (int i = 0; i < 1000; i++)
	{
		SeedArray.Add(Stream.RandRange(-1000, 1000));
		//GEngine->AddOnScreenDebugMessage(i, 10, FColor::Purple, FString::FromInt(SeedArray[i]));
	}

	/*AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]() {
		RecursiveRender(FVector2D(), 0);
	});*/
	//RenderSingleChunk(FVector2D(), 0);
}

// Called every frame
void ATerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//if (ActorToGenerateFrom == nullptr) { return; }
	//GEngine->AddOnScreenDebugMessage(1, 10, FColor::Purple, ActorToGenerateFrom->GetActorLocation().ToString());

	// Check if its time to do a render check and if one isn't already running
	if (GetWorld()->TimeSeconds >= NextChunkRenderCheck && !bCheckingRender) {
		NextChunkRenderCheck = GetWorld()->TimeSeconds + RenderCheckPeriod;
		bCheckingRender = true;

		FVector2D PointToRenderFrom = FVector2D(ActorToGenerateFrom->GetActorLocation().X, ActorToGenerateFrom->GetActorLocation().Y);
		RenderChunks(PointToRenderFrom);
	}
}

/*
Returns value from a perlin noise function, using a seed for point offset
*/
float ATerrain::GetNoiseValueAtPoint(FVector2D Point, float Frequency) {
	return FMath::PerlinNoise2D((Point + FVector2D(ExtractRandomNumber(), ExtractRandomNumber())) * Frequency);
}

/*
Returns data for a point using a seed and lookup table
*/
float ATerrain::GetTerrainPointData(FVector2D Point) {
	float ResultZ = 0;

	ResultZ += GetNoiseValueAtPoint(Point, 0.00008) * 5000;
	ResultZ += GetNoiseValueAtPoint(Point, 0.00008) * 5000;
	ResultZ += GetNoiseValueAtPoint(Point, 0.0000005) * 50000;
	ResultZ += GetNoiseValueAtPoint(Point, 0.0000005) * 50000;

	ResetLookupTable();
	return ResultZ;
}

/*
Extracts a random number from the lookup table and increments the look up index
*/
int ATerrain::ExtractRandomNumber() {
	i_Seed++;
	return SeedArray[i_Seed];
}

/*
Sets the look up table index counter to 0
*/
void ATerrain::ResetLookupTable() {
	i_Seed = 0;
}

/*
Renders chunks around a given point on a seperate thread, based on render distance and set LOD distances.
Algorithm renders chunks in a grid, not from the point of the player
*/
void ATerrain::RenderChunks(FVector2D From) {
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, From]() {
		// logic for rendering and deleting far chunks goes here

		// get array of chunk locations around player
		TArray<FVector2D> NearestChunks;
		GetNearestChunks(&NearestChunks);

		// Go through each chunk to check if it isnt already rendered, and render it, regardless of their LOD
		for (FVector2D ChunkToCheck : NearestChunks) {

			// Get LOD of what this chunk should be
			EChunkQuality LOD = EChunkQuality::High;

			// Get distance between chunk and observer
			float ChunkDistance = (ChunkToCheck - From).Length();

			if (ChunkDistance >= LowLODCutoffDist * totalChunkSize) {
				LOD = EChunkQuality::Low;
			}
			else if (ChunkDistance >= MediumLODCutoffDist * totalChunkSize) {
				LOD = EChunkQuality::Medium;

			}
			else if (ChunkDistance < MediumLODCutoffDist * totalChunkSize) {
				LOD = EChunkQuality::High;
			}

			// Check if chunk is found in memory
			FChunkRenderData* FoundChunk = CheckChunk(ChunkToCheck);

			// Haven't found chunk or the LODs dont match
			if (FoundChunk == nullptr) {
				GEngine->AddOnScreenDebugMessage(FMath::Rand(), 10, FColor::Magenta, FString("Creating mesh at ") + ChunkToCheck.ToString());
				RenderSingleChunk(ChunkToCheck, LOD);
			}
			else if (FoundChunk->ChunkQuality != LOD) {
				GEngine->AddOnScreenDebugMessage(FMath::Rand(), 10, FColor::Purple, FString("Updating mesh at ") + FoundChunk->Coordinates.ToString());
				UpdateChunk(ChunkToCheck, LOD);
			}
		}

		/*
		
		*/

		// finally end algorithm
		bCheckingRender = false;
	});
}

/*
Removes chunk at location
*/
void ATerrain::RemoveChunk(FVector2D ChunkLocationToRemove) {
	//// Check if chunk is found in memory
	//FChunkRenderData* FoundChunk = CheckChunk(ChunkLocationToRemove);

	//AsyncTask(ENamedThreads::GameThread, [this, FoundChunk]() {
	//	if (FoundChunk != nullptr) {
	//		Mesh->ClearMeshSection(FoundChunk->ChunkIndex);
	//		Chunks.RemoveAt(FoundChunk->ChunkIndex, EAllowShrinking::No);
	//	}
	//});
}

/*
Removes chunk with given chunk data, if chunk data is not null
*/
void ATerrain::RemoveChunk(FChunkRenderData *ChunkToRemove) {
	/*AsyncTask(ENamedThreads::GameThread, [this, ChunkToRemove]() {
		if (ChunkToRemove != nullptr) {
			Mesh->ClearMeshSection(ChunkToRemove->ChunkIndex);
			Chunks.RemoveAt(ChunkToRemove->ChunkIndex, EAllowShrinking::No);
		}
	});*/
}

/*
Gets array of chunks within render distances of the player
*/
void ATerrain::GetNearestChunks(TArray<FVector2D> *NearestChunks) {
	// check for chunks to be rendered around player
	FVector RenderPoint = ActorToGenerateFrom->GetActorLocation();

	// Round player location to nearest chunk
	FVector2D ClosestChunk = GetClosestChunkToPoint(FVector2D(RenderPoint.X, RenderPoint.Y));

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
FVector2D ATerrain::GetClosestChunkToPoint(FVector2D Point) {
	int RoundedX = FMath::Floor(Point.X / (totalChunkSize)) * totalChunkSize;
	int RoundedY = FMath::Floor(Point.Y / (totalChunkSize)) * totalChunkSize;
	return FVector2D(RoundedX, RoundedY);
}


/*
Recursively renders adjacent chunks
*/
void ATerrain::RecursiveRender(FVector2D ChunkCoord, int Iteration) {
	if (ChunkCoord.Length() >= totalChunkSize * 2) { return; }

	// Check if this chunk is already rendered
	for (int k = 0; k < Chunks.Num(); k++)
	{
		// Check if we found this chunk in the chunk array
		if (Chunks.IsValidIndex(k) && ChunkCoord == Chunks[k].Coordinates)
		{
			// We found chunk in array, its either rendering or rendered, so move on
			return;
		}
	}

	//RenderSingleChunk(ChunkCoord);

	TArray<FVector2D> AdjacentChunks;

	for (int i = -1; i <= 1; i++)
	{
		for (int j = -1; j <= 1; j++)
		{
			FVector2D AdjacentChunk = GetClosestChunkToPoint(ChunkCoord + FVector2D(i * totalChunkSize, j * totalChunkSize));
			// Check if this chunk is already rendered

			RecursiveRender(AdjacentChunk, Iteration + 1);
		}
	}
}

/*
Renders a single chunk at a given location if not already rendered
*/
void ATerrain::RenderSingleChunk(FVector2D ChunkCoord, EChunkQuality Quality) {

	// Get chunk mesh data
	FMeshData NewMeshData;
	GetChunkRenderData(&NewMeshData, ChunkCoord, Quality);

	// Finally create the mesh on proc mesh component on game thread
	AsyncTask(ENamedThreads::GameThread, [this, ChunkCoord, NewMeshData, Quality]() {
		// Get index place for this new chunk
		int DesignatedIndex = DesignateChunkIndex(ChunkCoord, Quality);

		Mesh->CreateMeshSection(DesignatedIndex, NewMeshData.Vertices, NewMeshData.Triangles, NewMeshData.Normals, NewMeshData.UVs, NewMeshData.Colors, NewMeshData.Tangents, Quality == EChunkQuality::High);
		Mesh->SetMaterial(DesignatedIndex, TerrainMaterial);
		Chunks[DesignatedIndex].RenderState = EChunkRenderState::Rendered;
	});
}

/*
Updates a chunk by deleting and regenerating it
*/
void ATerrain::UpdateChunk(FVector2D ChunkCoord, EChunkQuality NewQuality) {

	// Check the existing chunk
	FChunkRenderData* FoundChunk = CheckChunk(ChunkCoord);
	int FoundChunkIndex = FoundChunk->ChunkIndex;

	// LODs are the same or couldnt find chunk, nothing to do here
	if (FoundChunk == nullptr || FoundChunk->ChunkQuality == NewQuality) { return; }

	// Get chunk mesh data
	FMeshData NewMeshData;
	GetChunkRenderData(&NewMeshData, ChunkCoord, NewQuality);

	// Finally delete the original mesh and add as new mesh section
	AsyncTask(ENamedThreads::GameThread, [this, FoundChunkIndex, NewQuality, NewMeshData]() {

		// Delete old chunk
		Mesh->ClearMeshSection(FoundChunkIndex);
		Mesh->CreateMeshSection(FoundChunkIndex, NewMeshData.Vertices, NewMeshData.Triangles, NewMeshData.Normals, NewMeshData.UVs, NewMeshData.Colors, NewMeshData.Tangents, NewQuality == EChunkQuality::High);
		Mesh->SetMaterial(FoundChunkIndex, TerrainMaterial);
		Chunks[FoundChunkIndex].RenderState = EChunkRenderState::Rendered;
		Chunks[FoundChunkIndex].ChunkQuality = NewQuality;
	});
}

/*
Designates an index in the chunks array, returning the new found index
*/
int ATerrain::DesignateChunkIndex(FVector2D ChunkLocation, EChunkQuality ChunkQuality) {
	FChunkRenderData NewChunkData;

	// Add chunk data to array
	NewChunkData.Coordinates = ChunkLocation;
	NewChunkData.RenderState = EChunkRenderState::Rendering;
	NewChunkData.ChunkQuality = ChunkQuality;
	int DesignatedIndex = Chunks.Add(NewChunkData);
	Chunks[DesignatedIndex].ChunkIndex = DesignatedIndex;

	return DesignatedIndex;
}

/*
Get mesh data for a chunk at given location and LOD
*/
void ATerrain::GetChunkRenderData(FMeshData *MeshData, FVector2D ChunkCoord, EChunkQuality Quality)
{
	// Change the quality of the mesh generated
	int NewChunkSize = chunkSize;
	int NewTileSize = tileSize;
	GetChunkSizesFromQuality(Quality, &NewChunkSize, &NewTileSize);

	TArray<FVector> Vertices;
	TArray<int> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> Colors;

	// Add vertices
	for (int row_i = 0; row_i < NewChunkSize + 1; row_i++) {
		for (int col_i = 0; col_i < NewChunkSize + 1; col_i++) {
			FVector2D Point = ChunkCoord + FVector2D(NewTileSize * row_i, NewTileSize * col_i);
			FVector NewVertex = FVector(Point.X, Point.Y, GetTerrainPointData(Point));
			Vertices.Add(NewVertex);
		}
	}

	// Add triangles
	for (int row_i = 0; row_i < NewChunkSize; row_i++) {
		for (int col_i = 0; col_i < NewChunkSize; col_i++) {
			//// Add rightmost triangle
			Triangles.Add(row_i * (NewChunkSize + 1) + col_i);
			Triangles.Add(row_i * (NewChunkSize + 1) + col_i + 1);
			Triangles.Add((row_i + 1) * (NewChunkSize + 1) + col_i + 1);

			//// Add leftmost triangle
			Triangles.Add(row_i * (NewChunkSize + 1) + col_i);
			Triangles.Add((row_i + 1) * (NewChunkSize + 1) + col_i + 1);
			Triangles.Add((row_i + 1) * (NewChunkSize + 1) + col_i);
		}
	}

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

	MeshData->Vertices = Vertices;
	MeshData->UVs = UVs;
	MeshData->Colors = Colors;
	MeshData->Normals = Normals;
	MeshData->Tangents = Tangents;
	MeshData->Triangles = Triangles;
}

/*
Returns the index of found chunk and sets struct pointer to that
*/
FChunkRenderData* ATerrain::CheckChunk(FVector2D ChunkLocation) {
	for (int i = 0; i < Chunks.Num(); i++)
	{
		if (Chunks.IsValidIndex(i) && Chunks[i].Coordinates == ChunkLocation) {
			return &Chunks[i];
		}
	}

	return nullptr;
}

/*
Returns scaled chunk sizes and tile sizes with the LOD, should always try and equal to the initial totalChunkSize
*/
void ATerrain::GetChunkSizesFromQuality(EChunkQuality Quality, int *NewChunkSize, int *NewTileSize) {
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
	default:
		break;
	}
}


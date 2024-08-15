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
		RenderChunks();
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



void ATerrain::RenderChunks() {
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this]() {
		// logic for rendering and deleting far chunks goes here

		// check for chunks to be rendered around player
		FVector RenderPoint = ActorToGenerateFrom->GetActorLocation();

		// Round player location to nearest chunk
		FVector2D ClosestChunk = GetClosestChunkToPoint(FVector2D(RenderPoint.X, RenderPoint.Y));

		// get array of chunks around nearest chunk
		TArray<FVector2D> NearestChunks;
		for (int i = -ChunkRenderDistance; i < ChunkRenderDistance + 1; i++)
		{
			for (int j = -ChunkRenderDistance; j < ChunkRenderDistance + 1; j++)
			{
				NearestChunks.Add(ClosestChunk + FVector2D(i * totalChunkSize, j * totalChunkSize));
			}
		}

		// Go through each chunk to check if it isnt already rendered
		for (FVector2D ChunkToCheck : NearestChunks) {
			bool bFoundChunk = false;
			for (int i = 0; i < Chunks.Num(); i++)
			{
				// Check if we found this chunk in the chunk array
				if (Chunks.IsValidIndex(i) && ChunkToCheck == Chunks[i].Coordinates)
				{
					// We found chunk in array, its either rendering or rendered, so move on
					bFoundChunk = true;
					break;
				}
			}

			// Haven't found chunk, so it's not rendered
			if (!bFoundChunk) {
				/*AsyncTask(ENamedThreads::GameThread, [this, ChunkToCheck]() {
					GEngine->AddOnScreenDebugMessage(FMath::Rand(), 10, FColor::Purple, FString("Generating chunk at ") + ChunkToCheck.ToString());
				});*/
				// create new chunk information and designate index
				RenderSingleChunk(ChunkToCheck);
			}
		}

		// finally end algorithm
		bCheckingRender = false;
	});
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

	RenderSingleChunk(ChunkCoord);

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
Renders a single chunk at a given location
*/
void ATerrain::RenderSingleChunk(FVector2D ChunkCoord) {

	FChunkRenderData NewChunkData;

	// Change the quality of the mesh generated
	int NewChunkSize = chunkSize;
	int NewTileSize = tileSize;

	// Handle LOD based on chunk distance
	if (ChunkCoord.Length() >= LowLODCutoffDist * totalChunkSize) {
		GetChunkSizesFromQuality(EChunkQuality::Low, &NewChunkSize, &NewTileSize);
		NewChunkData.ChunkQuality = EChunkQuality::Low;
	}
	else if (ChunkCoord.Length() >= MediumLODCutoffDist * totalChunkSize) {
		GetChunkSizesFromQuality(EChunkQuality::Medium, &NewChunkSize, &NewTileSize);
		NewChunkData.ChunkQuality = EChunkQuality::Medium;
	}
	else if (ChunkCoord.Length() < MediumLODCutoffDist * totalChunkSize) {
		NewChunkData.ChunkQuality = EChunkQuality::High;
	}

	// Add chunk data to array
	NewChunkData.Coordinates = ChunkCoord;
	NewChunkData.RenderState = EChunkRenderState::Rendering;
	int DesignatedIndex = Chunks.Add(NewChunkData);

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
			Triangles.Add((row_i+1) * (NewChunkSize + 1) + col_i + 1);

			//// Add leftmost triangle
			Triangles.Add(row_i * (NewChunkSize + 1) + col_i);
			Triangles.Add((row_i + 1) * (NewChunkSize + 1) + col_i + 1);
			Triangles.Add((row_i + 1) * (NewChunkSize + 1) + col_i);
		}
	}

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
	//Mesh->AddCollisionConvexMesh(Vertices);

	// Finally create the mesh on proc mesh component on game thread
	AsyncTask(ENamedThreads::GameThread, [this, Vertices, Triangles, Normals, UVs, Tangents, Colors, DesignatedIndex, NewChunkData]() {
		Mesh->CreateMeshSection(DesignatedIndex, Vertices, Triangles, Normals, UVs, Colors, Tangents, NewChunkData.ChunkQuality == EChunkQuality::High);
		Mesh->SetMaterial(DesignatedIndex, TerrainMaterial);
	});
}

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


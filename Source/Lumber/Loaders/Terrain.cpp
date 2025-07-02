// Fill out your copyright notice in the Description page of Project Settings.


#include "Terrain.h"
#include "KismetProceduralMeshLibrary.h"
#include "TreeLoader.h"

#define GamePriority ENamedThreads::GameThread 
#define BackgroundPriority ENamedThreads::AnyBackgroundHiPriTask

// Sets default values
ATerrain::ATerrain()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Construct mesh component
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(FName("Mesh"));
	SetRootComponent(Mesh);
	
	CollisionMesh = CreateDefaultSubobject<UProceduralMeshComponent>(FName("CollisionMesh"));
	CollisionMesh->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);

	totalChunkSize = (chunkSize) * tileSize;
}

// Called when the game starts or when spawned
void ATerrain::BeginPlay()
{
	Super::BeginPlay();
	ActorToGenerateFrom = GetWorld()->GetFirstPlayerController()->GetPawn();
	totalChunkSize = (chunkSize) * tileSize;

	Mesh->bUseAsyncCooking = true;
	CollisionMesh->bUseAsyncCooking = true;

	// Set random seed for stream
	Stream.GenerateNewSeed();
	for (int i = 0; i < 1000; i++)
	{
		SeedArray.Add(Stream.RandRange(-1000, 1000));
	}
}

// Called every frame
void ATerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ActorToGenerateFrom != nullptr) {
		FVector Loc = ActorToGenerateFrom->GetActorLocation();
		ObserverLocation = FVector2D(Loc.X, Loc.Y) - FVector2D(totalChunkSize / 2, totalChunkSize / 2);
	}

	// Check if its time to do a render check and if one isn't already running
	if (GetWorld()->TimeSeconds >= NextChunkRenderCheck && !bCheckingRender) {
		NextChunkRenderCheck = GetWorld()->TimeSeconds + RenderCheckPeriod;
		bCheckingRender = true;
		RenderChunks(ObserverLocation);
	}
}

/*
Renders chunks around a given point on a seperate thread, based on render distance and set LOD distances.
Algorithm renders chunks in a grid, not from the point of the player
*/
void ATerrain::RenderChunks(FVector2D From) {
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

		RunJobs();

		// finally end algorithm
		bCheckingRender = false;
	});
}

/*
* Allocates new chunk index, then sets it to rendering state, then queues a task to load the chunk.
* Initiates chunk load procedure if chunk is not already loaded, loading everything including terrain and other data like trees
*/
void ATerrain::QueueChunkLoad(FVector2D ChunkLocation) {

	// Chunk is already loaded, so don't do anything
	if (GetChunk(ChunkLocation) != -1) { return; }

	EChunkQuality NewChunkQuality = GetTargetLODForChunk(ChunkLocation);
	int NewChunkIndex = FindOrCreateChunkData(ChunkLocation, NewChunkQuality);
	Chunks[NewChunkIndex].TerrainRenderState = EChunkRenderState::Rendering;
	Chunks[NewChunkIndex].ChunkQuality = NewChunkQuality;

	AddJob([this, NewChunkIndex]() {LoadChunk(NewChunkIndex); });
}

/*
Renders a single chunk given the index of the already created chunk data in the chunks array
*/
void ATerrain::LoadChunk(int ChunkDataIndex) {

	// Extract variables from ChunkData for easy access
	EChunkQuality ChunkTargetQuality = Chunks[ChunkDataIndex].ChunkQuality;
	FVector2D ChunkCoord = Chunks[ChunkDataIndex].ChunkLocation;

	// Here's where we would load other data, like terrain, generating trees, buildings, etc.

	// Start loading terrain for this chunk
	LoadChunkTerrain(ChunkDataIndex, ChunkTargetQuality, ChunkCoord);

	// Start loading trees for this chunk
	LoadChunkTrees(ChunkDataIndex, ChunkTargetQuality, ChunkCoord);

	// Set material on the game thread and set thte chunk data to be set as rendered
	AsyncTask(GamePriority, [this, ChunkDataIndex]() {
		Mesh->SetMaterial(ChunkDataIndex, TerrainMaterial);
		Chunks[ChunkDataIndex].TerrainRenderState = EChunkRenderState::Rendered;
	});
}

void ATerrain::LoadChunkTerrain(int ChunkDataIndex, EChunkQuality ChunkTargetQuality, FVector2D ChunkCoord) {

	// Create mesh data and optional collision data
	FMeshData NewMeshData;
	FMeshData NewMeshCollisionData;
	GetChunkRenderData(&NewMeshData, ChunkCoord, ChunkTargetQuality);
	if (ChunkTargetQuality == EChunkQuality::High) {
		GetChunkRenderData(&NewMeshCollisionData, ChunkCoord, EChunkQuality::Collision);
	}

	// Create mesh section for the mesh and collision mesh
	CreateMeshSection(Mesh, ChunkDataIndex, NewMeshData.Vertices, NewMeshData.Triangles, NewMeshData.Normals, NewMeshData.UVs, NewMeshData.Colors, NewMeshData.Tangents, false);
	if (ChunkTargetQuality == EChunkQuality::High) {
		CreateMeshSection(CollisionMesh, ChunkDataIndex, NewMeshCollisionData.Vertices, NewMeshCollisionData.Triangles, NewMeshCollisionData.Normals, NewMeshCollisionData.UVs, NewMeshCollisionData.Colors, NewMeshCollisionData.Tangents, true);
	}
}

void ATerrain::LoadChunkTrees(int ChunkDataIndex, EChunkQuality ChunkTargetQuality, FVector2D ChunkCoord) {
	Gamemode->GetTreeLoader()->GenerateTrees(ChunkDataIndex, ChunkCoord);
}

void ATerrain::LoadChunkBuildings() {

}

void ATerrain::OnFinishLoadedChunkTrees(int ChunkDataIndex)
{

}

/*
Deletes and reloades chunk
*/
void ATerrain::ReloadChunk(FVector2D ChunkLocation) {
	int ChunkIndex = GetChunk(ChunkLocation);
	if (ChunkIndex == -1) {
		QueueChunkLoad(ChunkLocation); 
		return;
	}

	ReloadChunk(ChunkIndex);
}

void ATerrain::ReloadChunk(int ChunkIndex) {
	if(ChunkValid(ChunkIndex)){
		EChunkQuality NewChunkQuality = GetTargetLODForChunk(Chunks[ChunkIndex].ChunkLocation);
		Chunks[ChunkIndex].TerrainRenderState = EChunkRenderState::Rendering;
		Chunks[ChunkIndex].ChunkQuality = NewChunkQuality;

		AddJob([this, ChunkIndex]() {LoadChunk(ChunkIndex); });
	}
}

/*
Checks if a given chunk needs to be reloaded, to change LOD, if it isn't rendering
Returns false if chunk can't be found
*/
bool ATerrain::CheckChunkForReloading(int ChunkIndex) {
	// Check if chunk is valid
	if (ChunkIndex == -1) { return false; }

	// Check if chunk isn't rendering
	if (Chunks[ChunkIndex].TerrainRenderState == EChunkRenderState::Rendering) { return false; }

	// Check if chunk has the same LOD
	if (Chunks[ChunkIndex].ChunkQuality == GetTargetLODForChunk(Chunks[ChunkIndex].ChunkLocation)) { return false; }

	// Otherwise chunk is ready to be reloaded
	return true;
}

bool ATerrain::CheckChunkForReloading(FVector2D ChunkLocation) {
	return CheckChunkForReloading(GetChunk(ChunkLocation));
}

/*
Returns the LOD level from distance between player and the chunk
*/
EChunkQuality ATerrain::GetTargetLODForChunk(FVector2D ChunkLocation) {
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
Add jobs to each batch without exeeding JobsPerBatch variable
*/
void ATerrain::AddJobs(const TArray<TFunction<void()>>& Jobs) {
	int NextJobToAdd = 0;
	int JobBatchIndex = 0;

	for (int i = 0; i < Jobs.Num(); i++)
	{
		AddJob(Jobs[i]);
	}
}

void ATerrain::AddJob(TFunction<void()> Job) {
	for (int i = 0; i < JobBatches.Num(); i++)
	{
		if (JobBatches[i].JobStatus == EJobStatus::NotRunning && JobBatches[i].Jobs.Num() < JobsPerBatch) {
			JobBatches[i].Jobs.Add(Job);
			return;
		}
	}

	FJobBatch NewBatch;
	NewBatch.Jobs.Add(Job);
	JobBatches.Add(NewBatch);
}

/*
Runs each batch of jobs on its seperate thread
*/
void ATerrain::RunJobs() {
	for (int i = 0; i < JobBatches.Num(); i++)
	{
		if (JobBatches[i].JobStatus == EJobStatus::NotRunning) {
			JobBatches[i].JobStatus = EJobStatus::Running;

			AsyncTask(BackgroundPriority, [this, i]() {
				for (int j = 0; j < JobBatches[i].Jobs.Num(); j++)
				{
					// Run the job in this batch
					JobBatches[i].Jobs[j]();
				}
				JobBatches[i].Jobs.Empty();
				JobBatches[i].JobStatus = EJobStatus::NotRunning;
			});
		}
	}
}

/*
Deletes a chunk given its index, by setting the ChunkIndex variable in the data at given index to -1, and
clearing the mesh section of that index.
*/
void ATerrain::DeleteChunkAtIndex(int ChunkIndex) {
	if (ChunkValid(ChunkIndex)) {
		AsyncTask(GamePriority, [this, ChunkIndex]() {
			Mesh->ClearMeshSection(ChunkIndex);
			if (Chunks[ChunkIndex].ChunkQuality == EChunkQuality::High) {
				CollisionMesh->ClearMeshSection(ChunkIndex);
			}
		});

		Chunks[ChunkIndex].ChunkIndex = -1;
	}
}



/*
Returns value from a perlin noise function, using a seed for point offset
*/
float ATerrain::GetNoiseValueAtPoint(FVector2D Point, float Frequency, int *i_Seed) {
	return FMath::PerlinNoise2D((Point + FVector2D(ExtractRandomNumber(i_Seed), ExtractRandomNumber(i_Seed))) * Frequency);
}

/*
Returns data for a point using a seed and lookup table
*/
float ATerrain::GetTerrainPointData(FVector2D Point) {
	int i_Seed = 0;
	float ResultZ = 0;

	// Bumps
	ResultZ += GetNoiseValueAtPoint(Point, 0.001, &i_Seed) * GetNoiseValueAtPoint(Point, 0.00001, &i_Seed) * 50;


	ResultZ += GetNoiseValueAtPoint(Point, 0.00001, &i_Seed) * 5000;
	ResultZ += GetNoiseValueAtPoint(Point, 0.0000005, &i_Seed) * 50000;
	ResultZ += GetNoiseValueAtPoint(Point, 0.0000005, &i_Seed) * 50000;

	// Mountains
	ResultZ += GetNoiseValueAtPoint(Point, 0.00005, &i_Seed) * 50000 * GetNoiseValueAtPoint(Point, 0.000004, &i_Seed);

	// Rivers
	ResultZ += FMath::Abs(GetNoiseValueAtPoint(Point, 0.000004, &i_Seed)) * 100000;

	// Mountain peaks
	ResultZ += FMath::Abs(GetNoiseValueAtPoint(Point, 0.000004, &i_Seed)) * -60000;


	//ResultZ += GetNoiseValueAtPoint(Point, 0.0000005, &i_Seed) * 1000000;
	//ResetLookupTable();
	return ResultZ;
}

/*
Extracts a random number from the lookup table and increments the look up index
*/
int ATerrain::ExtractRandomNumber(int *i_Seed) {
	(*i_Seed)++;
	return SeedArray[*i_Seed];
}

/*
Gets array of chunks within render distances of the player
*/
void ATerrain::GetNearestChunks(TArray<FVector2D> *NearestChunks) {

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
FVector2D ATerrain::GetClosestChunkToPoint(FVector2D Point) {
	int RoundedX = FMath::Floor(Point.X / (totalChunkSize)) * totalChunkSize;
	int RoundedY = FMath::Floor(Point.Y / (totalChunkSize)) * totalChunkSize;
	return FVector2D(RoundedX, RoundedY);
}


/*
Recursively renders adjacent chunks
*/
void ATerrain::RecursiveRender(FVector2D ChunkCoord, int Iteration) {
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
Given a chunk location, create and return a new chunk data or return the existing data
*/
int ATerrain::FindOrCreateChunkData(FVector2D ChunkLocation, EChunkQuality ChunkQuality) {

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
int ATerrain::AddNewChunkData(FChunkRenderData NewData) {

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
bool ATerrain::ChunkValid(int ChunkIndex) {
	// Chunk index is out of bounds, so chunk is invalid
	if (!Chunks.IsValidIndex(ChunkIndex)) { return false; }

	// Chunk index is valid, but set 
	if (Chunks[ChunkIndex].ChunkIndex == -1) { return false; }
	return true;
}

/*
Returns the index of a chunk, given its location and sets struct pointer to that, returns -1 if its not found
*/
int ATerrain::GetChunk(FVector2D ChunkLocation) {
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
int ATerrain::DesignateChunkIndex(FVector2D ChunkLocation, EChunkQuality ChunkQuality) {
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
	case Collision:
		*NewChunkSize = chunkSize;
		*NewTileSize = tileSize;
		break;
	default:
		break;
	}
}

/*
Own implementation of ProceduralMeshComponent's CreateMeshSection
*/
void ATerrain::CreateMeshSection(UProceduralMeshComponent *ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision)
{
	// Reset this section (in case it already existed)
	FProcMeshSection NewSection;

	// Copy data to vertex buffer
	const int32 NumVerts = Vertices.Num();
	NewSection.ProcVertexBuffer.Reset();
	NewSection.ProcVertexBuffer.AddUninitialized(NumVerts);
	for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
	{
		FProcMeshVertex& Vertex = NewSection.ProcVertexBuffer[VertIdx];

		Vertex.Position = Vertices[VertIdx];
		Vertex.Normal = (Normals.Num() == NumVerts) ? Normals[VertIdx] : FVector(0.f, 0.f, 1.f);
		Vertex.UV0 = (UV0.Num() == NumVerts) ? UV0[VertIdx] : FVector2D(0.f, 0.f);
		Vertex.UV1 = (UV1.Num() == NumVerts) ? UV1[VertIdx] : FVector2D(0.f, 0.f);
		Vertex.UV2 = (UV2.Num() == NumVerts) ? UV2[VertIdx] : FVector2D(0.f, 0.f);
		Vertex.UV3 = (UV3.Num() == NumVerts) ? UV3[VertIdx] : FVector2D(0.f, 0.f);
		Vertex.Color = (VertexColors.Num() == NumVerts) ? VertexColors[VertIdx] : FColor(255, 255, 255);
		Vertex.Tangent = (Tangents.Num() == NumVerts) ? Tangents[VertIdx] : FProcMeshTangent();

		// Update bounding box
		NewSection.SectionLocalBox += Vertex.Position;
	}

	// Get triangle indices, clamping to vertex range
	const int32 MaxIndex = NumVerts - 1;
	const auto GetTriIndices = [&Triangles, MaxIndex](int32 Idx)
		{
			return TTuple<int32, int32, int32>(FMath::Min(Triangles[Idx], MaxIndex),
				FMath::Min(Triangles[Idx + 1], MaxIndex),
				FMath::Min(Triangles[Idx + 2], MaxIndex));
		};

	const int32 NumTriIndices = (Triangles.Num() / 3) * 3; // Ensure number of triangle indices is multiple of three

	// Detect degenerate triangles, i.e. non-unique vertex indices within the same triangle
	int32 NumDegenerateTriangles = 0;
	for (int32 IndexIdx = 0; IndexIdx < NumTriIndices; IndexIdx += 3)
	{
		int32 a, b, c;
		Tie(a, b, c) = GetTriIndices(IndexIdx);
		NumDegenerateTriangles += a == b || a == c || b == c; //-V614
	}
	if (NumDegenerateTriangles > 0)
	{
		//UE_LOG(LogProceduralComponent, Warning, TEXT("Detected %d degenerate triangle%s with non-unique vertex indices for created mesh section in '%s'; degenerate triangles will be dropped."),
			//NumDegenerateTriangles, NumDegenerateTriangles > 1 ? TEXT("s") : TEXT(""), *GetFullName());
	}

	// Copy index buffer for non-degenerate triangles
	NewSection.ProcIndexBuffer.Reset();
	NewSection.ProcIndexBuffer.AddUninitialized(NumTriIndices - NumDegenerateTriangles * 3);
	int32 CopyIndexIdx = 0;
	for (int32 IndexIdx = 0; IndexIdx < NumTriIndices; IndexIdx += 3)
	{
		int32 a, b, c;
		Tie(a, b, c) = GetTriIndices(IndexIdx);

		if (a != b && a != c && b != c) //-V614
		{
			NewSection.ProcIndexBuffer[CopyIndexIdx++] = a;
			NewSection.ProcIndexBuffer[CopyIndexIdx++] = b;
			NewSection.ProcIndexBuffer[CopyIndexIdx++] = c;
		}
		else
		{
			--NumDegenerateTriangles;
		}
	}
	check(NumDegenerateTriangles == 0);
	check(CopyIndexIdx == NewSection.ProcIndexBuffer.Num());

	NewSection.bEnableCollision = bCreateCollision;

	AsyncTask(GamePriority, [this, ProcMesh, SectionIndex, NewSection]() {
		ProcMesh->SetProcMeshSection(SectionIndex, NewSection);
	});
}

void ATerrain::CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision)
{
	TArray<FVector2D> EmptyArray;
	CreateMeshSection(ProcMesh, SectionIndex, Vertices, Triangles, Normals, UV0, EmptyArray, EmptyArray, EmptyArray, VertexColors, Tangents, bCreateCollision);
}


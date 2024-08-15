// Fill out your copyright notice in the Description page of Project Settings.


#include "Terrain.h"
#include "KismetProceduralMeshLibrary.h"


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
		//GEngine->AddOnScreenDebugMessage(i, 10, FColor::Purple, FString::FromInt(SeedArray[i]));
	}

	/*AsyncTask(BackgroundPriority, [this]() {
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
	AsyncTask(BackgroundPriority, [this, From]() {
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
				GEngine->AddOnScreenDebugMessage(FMath::Rand(), RenderCheckPeriod * 2, FColor::MakeRandomColor(), FString("Creating mesh at ") + ChunkToCheck.ToString());
				RenderSingleChunk(ChunkToCheck, LOD);
			}
			else if (FoundChunk->ChunkQuality != LOD) {
				GEngine->AddOnScreenDebugMessage(FMath::Rand(), RenderCheckPeriod * 2, FColor::MakeRandomColor(), FString("Updating mesh at ") + FoundChunk->Coordinates.ToString());
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

	//AsyncTask(GamePriority, [this, FoundChunk]() {
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
	/*AsyncTask(GamePriority, [this, ChunkToRemove]() {
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

	// Create seperate mesh data for collision if LOD is high
	FMeshData NewMeshCollisionData;
	if (Quality == EChunkQuality::High) {
		GetChunkRenderData(&NewMeshCollisionData, ChunkCoord, EChunkQuality::Collision);
	}

	// Get index place for this new chunk
	int DesignatedIndex = DesignateChunkIndex(ChunkCoord, Quality);

	CreateMeshSection(Mesh, DesignatedIndex, NewMeshData.Vertices, NewMeshData.Triangles, NewMeshData.Normals, NewMeshData.UVs, NewMeshData.Colors, NewMeshData.Tangents, false);
	if (Quality == EChunkQuality::High) {
		CreateMeshSection(CollisionMesh, DesignatedIndex, NewMeshCollisionData.Vertices, NewMeshCollisionData.Triangles, NewMeshCollisionData.Normals, NewMeshCollisionData.UVs, NewMeshCollisionData.Colors, NewMeshCollisionData.Tangents, true);
	}

	//Mesh->SetMaterial(DesignatedIndex, TerrainMaterial);
	//Chunks[DesignatedIndex].RenderState = EChunkRenderState::Rendered;
	// Finally create the mesh on proc mesh component on game thread
	AsyncTask(GamePriority, [this, ChunkCoord, NewMeshData, Quality, NewMeshCollisionData, DesignatedIndex]() {
		Mesh->SetMaterial(DesignatedIndex, TerrainMaterial);
		
		
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
	FoundChunk->ChunkQuality = NewQuality;
	//FoundChunk->RenderState = EChunkRenderState::Rendering;

	// Get chunk mesh data
	FMeshData NewMeshData;
	GetChunkRenderData(&NewMeshData, ChunkCoord, NewQuality);

	// Create seperate mesh data for collision if LOD is high
	FMeshData NewMeshCollisionData;
	if (NewQuality == EChunkQuality::High) {
		GetChunkRenderData(&NewMeshCollisionData, ChunkCoord, EChunkQuality::Collision);
	}

	CreateMeshSection(Mesh, FoundChunkIndex, NewMeshData.Vertices, NewMeshData.Triangles, NewMeshData.Normals, NewMeshData.UVs, NewMeshData.Colors, NewMeshData.Tangents, false);

	if (NewQuality == EChunkQuality::High) {
		CreateMeshSection(CollisionMesh, FoundChunkIndex, NewMeshCollisionData.Vertices, NewMeshCollisionData.Triangles, NewMeshCollisionData.Normals, NewMeshCollisionData.UVs, NewMeshCollisionData.Colors, NewMeshCollisionData.Tangents, true);
	}

	//FoundChunk->RenderState = EChunkRenderState::Rendered;

	// Finally delete the original mesh and add as new mesh section
	AsyncTask(GamePriority, [this, FoundChunkIndex, NewQuality, NewMeshData, NewMeshCollisionData]() {
		Mesh->SetMaterial(FoundChunkIndex, TerrainMaterial);

		//// Delete old chunk
		//Mesh->ClearMeshSection(FoundChunkIndex);
		//CollisionMesh->ClearMeshSection(FoundChunkIndex);

		
		//Chunks[FoundChunkIndex].RenderState = EChunkRenderState::Rendered;
		//Chunks[FoundChunkIndex].ChunkQuality = NewQuality;
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


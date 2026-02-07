// Fill out your copyright notice in the Description page of Project Settings.


#include "TerrainLoader.h"
#include "KismetProceduralMeshLibrary.h"
#include "TreeLoader.h"
#include "ProceduralMeshComponent.h"
#include "ChunkLoader.h"

// Sets default values
ATerrainLoader::ATerrainLoader()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(FName("Mesh"));
    SetRootComponent(Mesh);

    CollisionMesh = CreateDefaultSubobject<UProceduralMeshComponent>(FName("CollisionMesh"));
    CollisionMesh->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
}

// Called when the game starts or when spawned
void ATerrainLoader::BeginPlay()
{
	Super::BeginPlay();

	Mesh->bUseAsyncCooking = true;
	CollisionMesh->bUseAsyncCooking = true;

	
	// Set random seed for stream
	Stream.GenerateNewSeed();
	for (int i = 0; i < 1000; i++)
	{
		SeedArray.Add(Stream.RandRange(-1000, 1000));
	}
}

void ATerrainLoader::LoadChunkTerrain(int ChunkDataIndex, EChunkQuality ChunkTargetQuality, FVector2D ChunkCoord) {

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

/*
Returns data for a point using a seed
*/
float ATerrainLoader::GetTerrainPointData(FVector2D Point) {
	int i_Seed = 0;
	float ResultZ = 0;

	for (const FNoiseLayer Layer : LoadedWorldSettings.MountainLayer) {

		// Stretch or translate point for the perlin noise function
		FVector2D ProcessedPoint = Point;
		ProcessedPoint.X = ProcessedPoint.X * Layer.XScale;
		ProcessedPoint.Y = ProcessedPoint.Y * Layer.YScale;
		ProcessedPoint.X += Layer.XOffset;
		ProcessedPoint.Y += Layer.YOffset;

		// Amplify the point
		float NewResultZ = FMath::PerlinNoise2D(ProcessedPoint);
		NewResultZ = NewResultZ * Layer.Gain;

		// Operation to the resulting Z point
		switch (Layer.OperationType.GetValue())
		{	
		case Additive:
			ResultZ += NewResultZ;
			break;
		case Multiplicative:
			ResultZ = ResultZ * NewResultZ;
			break;
		default:
			break;
		}
	}



	//// Bumps
	//ResultZ += GetNoiseValueAtPoint(Point, 0.001, &i_Seed) * GetNoiseValueAtPoint(Point, 0.00001, &i_Seed) * 50;
	//ResultZ += GetNoiseValueAtPoint(Point, 0.00001, &i_Seed) * 5000;
	//ResultZ += GetNoiseValueAtPoint(Point, 0.0000005, &i_Seed) * 50000;
	//ResultZ += GetNoiseValueAtPoint(Point, 0.0000005, &i_Seed) * 50000;

	//// Mountains
	//ResultZ += GetNoiseValueAtPoint(Point, 0.00005, &i_Seed) * 50000 * GetNoiseValueAtPoint(Point, 0.000004, &i_Seed);

	//// Rivers
	//ResultZ += FMath::Abs(GetNoiseValueAtPoint(Point, 0.000004, &i_Seed)) * 100000;

	//// Mountain peaks
	//ResultZ += FMath::Abs(GetNoiseValueAtPoint(Point, 0.000004, &i_Seed)) * -60000;


	//ResultZ += GetNoiseValueAtPoint(Point, 0.0000005, &i_Seed) * 1000000;
	//ResetLookupTable();
	return ResultZ;
}

/*
	We want to, for each point, use the perlin noise function multiple times according to the terrain settings
*/

/*
Returns value from a perlin noise function, using a seed for point offset
*/
float ATerrainLoader::GetNoiseValueAtPoint(FVector2D Point, float Frequency, int* i_Seed) {
	return FMath::PerlinNoise2D((Point + FVector2D(ExtractRandomNumber(i_Seed), ExtractRandomNumber(i_Seed))) * Frequency);
}

/*
Extracts a random number from the lookup table and increments the look up index
*/
int ATerrainLoader::ExtractRandomNumber(int* i_Seed) {
	(*i_Seed)++;
	return SeedArray[*i_Seed];
}


/*
Get mesh data for a chunk at given location and LOD
*/
void ATerrainLoader::GetChunkRenderData(FMeshData* MeshData, FVector2D ChunkCoord, EChunkQuality Quality)
{
	// Change the quality of the mesh generated
	int NewChunkSize = Gamemode->GetChunkLoader()->chunkSize;
	int NewTileSize = Gamemode->GetChunkLoader()->tileSize;
	Gamemode->GetChunkLoader()->GetChunkSizesFromQuality(Quality, &NewChunkSize, &NewTileSize);

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
Own implementation of ProceduralMeshComponent's CreateMeshSection
*/
void ATerrainLoader::CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision)
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
		ProcMesh->SetMaterial(SectionIndex, Gamemode->TerrainMaterial);
	});
}

void ATerrainLoader::CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision)
{
	TArray<FVector2D> EmptyArray;
	CreateMeshSection(ProcMesh, SectionIndex, Vertices, Triangles, Normals, UV0, EmptyArray, EmptyArray, EmptyArray, VertexColors, Tangents, bCreateCollision);
}

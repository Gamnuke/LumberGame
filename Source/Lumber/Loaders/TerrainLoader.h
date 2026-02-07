// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Loader.h"
#include "ProceduralMeshComponent.h"
#include "../Serialization/MyWorld.h"
#include "TerrainLoader.generated.h"

struct FMyWorldSettings;

USTRUCT()
struct FMeshData {
	GENERATED_BODY()
	TArray<FVector> Vertices;
	TArray<int> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> Colors;
};

UCLASS()
class LUMBER_API ATerrainLoader : public ALoader
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UProceduralMeshComponent* Mesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UProceduralMeshComponent* CollisionMesh;

public:
	ATerrainLoader();

	void GetChunkRenderData(FMeshData* MeshData, FVector2D ChunkCoord, EChunkQuality Quality);

	void CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FVector2D>& UV1, const TArray<FVector2D>& UV2, const TArray<FVector2D>& UV3, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);

	void CreateMeshSection(UProceduralMeshComponent* ProcMesh, int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);
	
	float GetTerrainPointData(FVector2D Point);

	void LoadChunkTerrain(int ChunkDataIndex, EChunkQuality ChunkTargetQuality, FVector2D ChunkCoord);

	int ExtractRandomNumber(int* i_Seed);

	float GetNoiseValueAtPoint(FVector2D Point, float Frequency, int* i_Seed);

	// Stream for psudorandom numbers
	FRandomStream Stream;

	TArray<int> SeedArray;

	FMyWorldSettings LoadedWorldSettings;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay();

	
};

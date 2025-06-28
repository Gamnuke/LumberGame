// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Serialization/SerializableObject.h"
#include "LogData.h"
#include "ProceduralMeshComponent.h"
#include "TreeRoot.generated.h"

class ATree;
class LogData;
class UProceduralMeshComponent;
struct FRandomStream;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LUMBER_API ATreeRoot : public AActor, public ISerializableObject
{
	GENERATED_BODY()

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Sets default values for this component's properties
	ATreeRoot();

	void OnFinishGeneration();

	// Called every frame
	virtual void Tick(float DeltaTime) override;


	void GenerateTree();

	void SpawnLogsRecursive(ULogData* NextData, ATree* ParentTree);

	void GenerateMeshOnlyRecursive(ULogData* NextData);

	void GenerateTreeData();

public:	
	ATree* Root;

	int TreeSeed;

	TSubclassOf<ATree> TreeClass;

	FRandomStream NumberStream;

	double StartGenerationTime;

	UProceduralMeshComponent* MaskMesh;
	int nextMeshSectionIndex = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FVector> Vertices;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<int32> Triangles;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FVector2D> UVs;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> Colors;

public:
	FJsonObject SerializeObject() override;
	void DeserializeAndLoadObject(FJsonObject ObjectData) override;

	TArray<ULogData*> AllLogData;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FData InitialTreeData;

public:

	void BuildTreeData(int MaxDepth, int CurrentDepth, ULogData* Parent, bool Extension);
	static int RandRange(int Min, int Max, FRandomStream& Stream);
	static float FRandRange(float Min, float Max, FRandomStream& Stream);

};

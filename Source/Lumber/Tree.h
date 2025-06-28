// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"
#include "ProceduralMeshComponent.h"
#include "TreeClasses/LogData.h"
#include "Tree.generated.h"

class UNiagaraComponent;
class UProceduralMeshComponent;
class UTreeMesh;
class UPaperSprite;
class USceneComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class UBoxComponent;
class ATreeRoot;
struct FRandomStream;

USTRUCT(BlueprintType)
struct FCutData {
	GENERATED_BODY()

	// distance to this cut from the actor location in relative space (negative if under actor location, positive if over)
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	float RelativeDistance;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	int CutAmount;
};

USTRUCT()
struct FProcMeshInfo {
	GENERATED_BODY()

	int Index;

	TArray<FVector> Vertices;
	TArray<int> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> MeshTangents;
};

UCLASS()
class LUMBER_API ATree : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATree();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	FVector TraceMesh(FVector Start, FVector End, UProceduralMeshComponent* ProcMesh);

public:	
	// Tree settings

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UNiagaraComponent* ParticleSystem;

	static const int LeafMeshIndex = 0;
	static const int LogMeshIndex = 1;
	static const int RingMeshIndex = 2;

	FData ThisLogData;

	// Asyncing tasks make trees unique regardless of seed because it accesses a random value at different times
	// therefore skewing actual random variables.
	// Solution to make a stream for each Async task

public:
	// Functions
	void SetupTree(ATree* Tree, int NewBranchHeight, int NewBranchWidth);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void GenerateMeshRecursive(ATree* BranchToGenerate);

	void CutTree(FVector CutLocation, UProceduralMeshComponent* ProcMesh, ATree* Tree);
	void WeldChildrenRecursive(ATree* Root, ATree* NextTree);

	void RemoveLeavesRecursive(ATree* Root, ATree* NextTree);

	static FProcMeshInfo CreateMeshData(bool bBuildLeaves, FData NewLogData, FRandomStream NumberStream, FVector LocalOrigin, FVector UpVector);

	void BuildTreeMesh(bool bBuildLeaves);
	void MakeLeaves();
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		UProceduralMeshComponent* Mesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		USceneComponent* SceneComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		UCapsuleComponent* CapsuleComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		UBoxComponent* BoxCollision;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool FirstTree = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		UPaperSprite *Sprite;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
		int BranchHeight;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
		bool bPartOfRoot;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
		ATreeRoot* TreeRoot;





public:
	// Variables for self


	// Existing cuts for this branch in relative position
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	TArray<FCutData> ExistingCuts;

	// Mesh information stored for mesh generation
	FProcMeshInfo BranchMeshInfo;
	FProcMeshInfo LeafMeshInfo;
	bool bMakeLeaves = false;

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"
#include "ProceduralMeshComponent.h"
#include "LogData.h"
#include "../Loaders/Loader.h"
#include "Tree.generated.h"

class UNiagaraComponent;
class UProceduralMeshComponent;
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

	/*
		Recursively goes through branches' children and generates mesh using the mesh information stored in each branch
	*/
	void GenerateMeshRecursive(ATree* BranchToGenerate);

	void CutTree(FVector CutLocation, UProceduralMeshComponent* ProcMesh, ATree* Tree);
	void WeldChildrenRecursive(ATree* Root, ATree* NextTree);

	void RemoveLeavesRecursive(ATree* Root, ATree* NextTree);

	/*
		Only returns FProcMeshInfo mesh data of a log, given quality and local offset, used for far LOD purposes
	*/
	static FProcMeshInfo CreateMeshData(bool bBuildLeaves, FData NewLogData, FRandomStream NumberStream, EChunkQuality RenderQuality, FVector LocalOrigin, FVector UpVector);
	static FProcMeshInfo CreateLeavesMeshData(FData NewLogData, FRandomStream NumberStream, FVector LocalOrigin, FVector UpVector);

	/*
		Creates mesh data for this log and applies it to this actor, used for actual gameplay, ie not just for rendering from afar
	*/
	void BuildTreeMesh(bool bBuildLeaves);
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
		bool bPartOfRoot;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
		ATreeRoot* TreeRoot;


private:
	/*
		Returns full high quality version of branch
	*/
	static FProcMeshInfo GetHighQualityMesh(bool bBuildLeaves, FData NewLogData, FRandomStream NumberStream, FVector LocalOrigin = FVector::ZeroVector, FVector UpVector = FVector::UpVector);

	/*
		Returns a low quality version of branch, with only two vertex rows (one bottom one top)
	*/
	static FProcMeshInfo GetLowQualityMesh(bool bBuildLeaves, FData NewLogData, FRandomStream NumberStream, FVector LocalOrigin = FVector::ZeroVector, FVector UpVector = FVector::UpVector);


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

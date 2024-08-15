// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"
#include "ProceduralMeshComponent.h"
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

	void SetupTree(ATree* Tree, int NewBranchHeight, int NewBranchWidth);
public:	
	// Tree settings

	// How many rows of vertices for each branch
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int ROWS = 10;

	// Thickness of the branch
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int WIDTH = 100;

	// Gap between each row of vertices
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int SECTION_HEIGHT = 50;

	// Angle between each vertex in one row (must be divisible into 360)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int INCREMENT = 45;

	// Randomness of the vertices' location
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int R = 10;

	// Minimum length of a cut branch that allows the branch to be cut
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float MINCUTLENGTH = 25;

	// Size of the leaf
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float LeafSize = 150;

	// How spread out the leaves are
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float RandLeafThreshold = 0.6;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Chance)
	int SideStemChance = 50;

	// Min chance for new branches
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int BranchNumMin = 1;

	// Max chance for new branches
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int BranchNumMax = 5;

	// chance for branch to extend instead of creating new branches as percentage
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Chance)
	int ExtendChance = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int NumLeaves = 50;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int Seed;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHasLeaves = false;

	// allows the trunk of the tree to be changed, 1 = same size as branches
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float TrunkHeightMultiplier = 1.5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UNiagaraComponent* ParticleSystem;

	// How straight upwards the tree will be (lower values means straighter tree)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float StraightAmount = 0.5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMaterialInterface* LogMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMaterialInterface* LeafMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMaterialInterface* CrossSectionMaterial;

	int LeafMeshIndex = 0;
	int LogMeshIndex = 1;
	int RingMeshIndex = 2;

	// Asyncing tasks make trees unique regardless of seed because it accesses a random value at different times
	// therefore skewing actual random variables.
	// Solution to make a stream for each Async task

public:
	// Functions
 
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void GenerateMeshRecursive(ATree* BranchToGenerate);

	void CutTree(FVector CutLocation, UProceduralMeshComponent* ProcMesh, ATree* Tree);
	void WeldChildrenRecursive(ATree* Root, ATree* NextTree);

	void RemoveLeavesRecursive(ATree* Root, ATree* NextTree);

	void BuildTreeMesh(bool bBuildLeaves);
	void MakeLeaves();
	void BuildTreeMeshRecursive(int MaxDepth, int CurrentDepth, ATree* Parent, bool Extension);

	int RandRange(int Min, int Max, FRandomStream& Stream);
	float FRandRange(float Min, float Max, FRandomStream& Stream);
	
	void StartGeneration();
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

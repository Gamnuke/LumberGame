// Fill out your copyright notice in the Description page of Project Settings.


#include "TreeRoot.h"
#include "Tree.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KismetProceduralMeshLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "PaperSpriteComponent.h"
#include "SceneManagement.h"
#include "Async/Async.h"
#include "Experimental/Async/AwaitableTask.h"
#include "Math/RandomStream.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Math/UnrealMathUtility.h"
#include "LogData.h"
#include "../Loaders/ChunkLoader.h"

class ULogData;
#define GamePriority ENamedThreads::GameThread 
#define BackgroundPriority ENamedThreads::AnyBackgroundHiPriTask

// Sets default values for this component's properties
ATreeRoot::ATreeRoot()
{
	MaskMesh = CreateDefaultSubobject<UProceduralMeshComponent>(FName("TreeMeshComponent"));
	/*MaskMesh->bUseComplexAsSimpleCollision = false;
	MaskMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MaskMesh->SetSimulatePhysics(false);*/

	MaskMesh->bUseComplexAsSimpleCollision = true;
	MaskMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MaskMesh->SetSimulatePhysics(false);
}

// Called every frame
void ATreeRoot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

FJsonObject ATreeRoot::SerializeObject()
{
	return FJsonObject();
}

void ATreeRoot::DeserializeAndLoadObject(FJsonObject ObjectData)
{
}

// Called when the game starts
void ATreeRoot::BeginPlay()
{
	Super::BeginPlay();
	
}

/*
Main function called to generate tree
*/
void ATreeRoot::GenerateTree(EChunkQuality TreeQuality, FTreeChunkRenderData* NewAssignedTreeLoaderChunk, bool* NewAssignedTreeLoaderTree) {
	AssignedTreeLoaderChunk = NewAssignedTreeLoaderChunk;
	AssignedTreeLoaderTree = NewAssignedTreeLoaderTree;

	// Sets up stream with given seed
	NumberStream = FRandomStream(TreeSeed);

	// Generates LogData
	AllLogData = GenerateTreeData();

	switch (TreeQuality)
	{
	case Low:
		AsyncTask(BackgroundPriority, [this]() {

			GenerateMeshOnlyRecursive(AllLogData[0]);

			AsyncTask(GamePriority, [this]() {

				// Create Log mesh
				MaskMesh->CreateMeshSection(
					0,
					LogMeshInfo.Vertices,
					LogMeshInfo.Triangles,
					LogMeshInfo.Normals,
					LogMeshInfo.UVs,
					LogMeshInfo.Colors,
					LogMeshInfo.MeshTangents,
					true
				);
				MaskMesh->SetMaterial(0, InitialTreeData.LogMaterial);

				// Create leaves mesh
				MaskMesh->CreateMeshSection(
					1,
					LeavesMeshInfo.Vertices,
					LeavesMeshInfo.Triangles,
					LeavesMeshInfo.Normals,
					LeavesMeshInfo.UVs,
					LeavesMeshInfo.Colors,
					LeavesMeshInfo.MeshTangents,
					false
				);
				MaskMesh->SetMaterial(1, InitialTreeData.LeafMaterial);


			});
		});
		break;

	case High:

		SpawnLogsRecursive(AllLogData[0], nullptr);

		break;
	default:
		break;
	}
}

/*
Generates data for tree, including position, rotation of branches
*/
TArray<ULogData*> ATreeRoot::GenerateTreeData() {

	// Create new LogData array
	TArray<ULogData*> NewLogDatas = TArray<ULogData*>();

	// Sets up stream with given seed
	NumberStream = FRandomStream(TreeSeed);

	// Setup first log data array entry
	ULogData* FirstLogData = NewObject<ULogData>();
	FirstLogData->Data = InitialTreeData;

	FirstLogData->Data.ROWS *= FirstLogData->Data.TrunkHeightMultiplier;
	FirstLogData->Data.BranchHeight = (FirstLogData->Data.ROWS - 1) * FirstLogData->Data.SECTION_HEIGHT;

	FirstLogData->Data.bPartOfRoot = true;
	FirstLogData->Data.LocalLocation = FVector::UpVector * FirstLogData->Data.BranchHeight / 2;
	NewLogDatas.Add(FirstLogData);

	BuildTreeData(4, 0, FirstLogData, true, NewLogDatas);

	return NewLogDatas;
}

void ATreeRoot::BuildTreeData(int MaxDepth, int CurrentDepth, ULogData* Parent, bool Extension, TArray<ULogData*>& LogDatas) {
	// stop building the tree if this branch exceeds the max depth
	if (CurrentDepth > MaxDepth) { return; }

	// get random number for how many branches branch off from the parent branch 
	int iRange = RandRange(Parent->Data.BranchNumMin, Parent->Data.BranchNumMax - Parent->Data.BranchNumMax * (CurrentDepth / MaxDepth), NumberStream);
	if (Extension) {
		iRange = 1;
	}

	// for each new branch...
	for (int i = 0; i < iRange; i++) {

		// chance to spawn a branch at side of log instead of end of log
		bool SideStem = RandRange(0, 100, NumberStream) <= Parent->Data.SideStemChance;
		float StemAmount = RandRange(0.0f, 1.0f, NumberStream);
		if (!SideStem || CurrentDepth <= 2) {
			StemAmount = 1;
		}
		StemAmount = 1;

		// random angle for new branch
		float Randomness = (60 - (60 * (CurrentDepth / MaxDepth)) * 0.7) * (CurrentDepth * 2 / MaxDepth + Parent->Data.StraightAmount);
		float RandPitch = FRandRange(-Randomness, Randomness, NumberStream);
		float RandYaw = FRandRange(-Randomness, Randomness, NumberStream);
		float RandRow = FRandRange(-Randomness, Randomness, NumberStream);

		// spawns new branch and setups data
		ULogData* NewLogData = NewObject<ULogData>();
		NewLogData->Data = InitialTreeData;
		NewLogData->Parent = Parent;
		NewLogData->Data.WIDTH = FMath::Clamp(InitialTreeData.WIDTH * (MaxDepth - CurrentDepth) / MaxDepth, InitialTreeData.WIDTH / 4, InitialTreeData.WIDTH);
		NewLogData->Data.ROWS = FRandRange(InitialTreeData.ROWS / (CurrentDepth + 1), InitialTreeData.ROWS, NumberStream);
		NewLogData->Data.BranchHeight = (NewLogData->Data.ROWS - 1) * NewLogData->Data.SECTION_HEIGHT;


		FVector NewUpVector = (Parent->Data.UpVector.Rotation() + FRotator(RandPitch, RandYaw, RandRow)).Vector();

		// First move new log up by parent's branch height
		FVector NewTreeLoc = Parent->Data.LocalLocation + Parent->Data.UpVector * (Parent->Data.BranchHeight / 2) * StemAmount;

		// Then move new log up by this new log's branch height
		NewTreeLoc = NewTreeLoc + NewUpVector * (NewLogData->Data.BranchHeight / 2) * StemAmount;

		NewLogData->Data.LocalLocation = NewTreeLoc;
		NewLogData->Data.UpVector = NewUpVector;

		// Add this as parent's child
		Parent->Children.Add(NewLogData);

		//chance to extend this branch, recursing with the same depth
		bool ExtendThis = (RandRange(0, 100, NumberStream) <= NewLogData->Data.ExtendChance);
		if (CurrentDepth == MaxDepth && !ExtendThis) {
			NewLogData->Data.bMakeLeaves = true;

		}
		else {
			NewLogData->Data.bMakeLeaves = false;

		}
		LogDatas.Add(NewLogData);

		if (ExtendThis) {
			//Cast<ALumberGameMode>(GetWorld()->GetAuthGameMode())->TaskQueue.Enqueue();
			BuildTreeData(MaxDepth, CurrentDepth, NewLogData, true, LogDatas);
		}
		else {
			//Cast<ALumberGameMode>(GetWorld()->GetAuthGameMode())->TaskQueue.Enqueue();
			BuildTreeData(MaxDepth, CurrentDepth + 1, NewLogData, false, LogDatas);
		}


	}
}

void ATreeRoot::GenerateMeshOnlyRecursive(ULogData* NextData) {
	FData Data = NextData->Data;

	FProcMeshInfo NewMeshInfo = ATree::CreateMeshData(false, Data, NumberStream, EChunkQuality::Low, Data.LocalLocation - Data.UpVector * Data.BranchHeight / 2, Data.UpVector);

	// We need to shift indexes of all triangles by the number of vertices already in the array,
	// so the new triangles' index correspond to the correct vertex
	for (int i = 0; i < NewMeshInfo.Triangles.Num(); i++)
	{
		NewMeshInfo.Triangles[i] += LogMeshInfo.Vertices.Num();
	}

	LogMeshInfo.Vertices.Append(NewMeshInfo.Vertices);
	LogMeshInfo.Triangles.Append(NewMeshInfo.Triangles);
	LogMeshInfo.Normals.Append(NewMeshInfo.Normals);
	LogMeshInfo.UVs.Append(NewMeshInfo.UVs);
	LogMeshInfo.Colors.Append(NewMeshInfo.Colors);
	LogMeshInfo.MeshTangents.Append(NewMeshInfo.MeshTangents);

	if (Data.bMakeLeaves) {
		FProcMeshInfo NewLeavesMeshInfo = ATree::CreateLeavesMeshData(Data, NumberStream, Data.LocalLocation, Data.UpVector);

		for (int i = 0; i < NewLeavesMeshInfo.Triangles.Num(); i++)
		{
			NewLeavesMeshInfo.Triangles[i] += LeavesMeshInfo.Vertices.Num();
		}

		LeavesMeshInfo.Vertices.Append(NewLeavesMeshInfo.Vertices);
		LeavesMeshInfo.Triangles.Append(NewLeavesMeshInfo.Triangles);
		LeavesMeshInfo.Normals.Append(NewLeavesMeshInfo.Normals);
		LeavesMeshInfo.UVs.Append(NewLeavesMeshInfo.UVs);
		LeavesMeshInfo.Colors.Append(NewLeavesMeshInfo.Colors);
		LeavesMeshInfo.MeshTangents.Append(NewLeavesMeshInfo.MeshTangents);

	}

	for (ULogData* Child : NextData->Children) {
		GenerateMeshOnlyRecursive(Child);
	}

}

void ATreeRoot::SpawnLogsRecursive(ULogData *NextData, ATree* ParentTree) {
	FData Data = NextData->Data;
	FVector NewSpawnLocation = GetActorLocation() + Data.LocalLocation;

	FVector NewLogLocalLocation = Data.LocalLocation;
	FRotator NewLogRotation = Data.UpVector.Rotation();
	NewLogRotation.Pitch += -90;
	ATree* NewTree = GetWorld()->SpawnActor<ATree>(TreeClass, NewSpawnLocation, NewLogRotation);
	NewTree->TreeRoot = this;
	NewTree->ThisLogData = NextData->Data;
	NewTree->BuildTreeMesh(Data.bMakeLeaves);

	NewTree->SetupTree(NewTree, Data.BranchHeight, Data.WIDTH);

	if (ParentTree != nullptr) {
		NewTree->BoxCollision->WeldTo(ParentTree->BoxCollision);
	}

	for (ULogData* Child : NextData->Children) {
		SpawnLogsRecursive(Child, NewTree);
	}

	if (NextData->Parent != nullptr) {
		DrawDebugLine(GetWorld(), NewSpawnLocation + Data.UpVector * Data.BranchHeight / 2, NewSpawnLocation - Data.UpVector * Data.BranchHeight / 2, FColor::Blue, false, 100, 0, 10);
	}
}

/*
Called by first branch to notify that the tree is fully generated and rendered
*/
void ATreeRoot::OnFinishGeneration() {
	double GenerationDuration = FPlatformTime::Seconds() - StartGenerationTime;
	GEngine->AddOnScreenDebugMessage(FMath::Rand(), 20, FColor::Green, FString() + "Took " + FString::SanitizeFloat(GenerationDuration) + "s to make tree");
}

int ATreeRoot::RandRange(int Min, int Max, FRandomStream& Stream) {
	int NewNum = UKismetMathLibrary::RandomIntegerInRangeFromStream(Stream, Min, Max);
	//GEngine->AddOnScreenDebugMessage(FMath::Rand(), 10, FColor::Green, FString::FromInt(NewNum));
	return NewNum;
	//return RandomStream.RandRange(Min, Max);
}

float ATreeRoot::FRandRange(float Min, float Max, FRandomStream& Stream) {
	float NewNum = UKismetMathLibrary::RandomFloatInRangeFromStream(Stream, Min, Max);
	//GEngine->AddOnScreenDebugMessage(FMath::Rand(), 10, FColor::Green, FString::SanitizeFloat(NewNum));
	return NewNum;
	//return RandomStream.FRandRange(Min, Max);
}

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Serialization/SerializableObject.h"
#include "LogData.h"
#include "ProceduralMeshComponent.h"
#include "../Tree.h"
#include "../TerrainClasses/Terrain.h"
#include "TreeRoot.generated.h"

class ATree;
class LogData;
class UProceduralMeshComponent;
struct FRandomStream;
struct FProcMeshInfo;

/*
	Class that is meant to be a container for logs, also acts as a one-point to enable LOD and to make rendering more efficient
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LUMBER_API ATreeRoot : public AActor, public ISerializableObject
{
	GENERATED_BODY()

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:	

	// Sets default values for this component's properties
	ATreeRoot();

public:
	/*
		Generates tree with given quality if not already generated
	*/
	void GenerateTree(EChunkQuality TreeQuality);

	/*
		Initiates the recursive call of BuildTreeData() and returns a LogData array
	*/
	TArray<ULogData*> GenerateTreeData();
	
	/*
		Called by first branch to notify that the tree is fully generated and rendered
	*/
	void OnFinishGeneration();


/*
	Generation functions
*/
private:

	/*
		Recursively builds information of each log in the tree, excluding mesh data and populates LogDatas array
	*/
	void BuildTreeData(int MaxDepth, int CurrentDepth, ULogData* Parent, bool Extension, TArray<ULogData*>& LogDatas);
	
	/*
		Builds only the tree's mesh without any collision or new actors, very computationally cheap but should
		be used for outer unimportant chunks
	*/
	void GenerateMeshOnlyRecursive(ULogData* NextData);

	/*
		Spawns the physical tree, very computationally expensive and should be used in moderation
	*/
	void SpawnLogsRecursive(ULogData* NextData, ATree* ParentTree);

/*
	Getter functions
*/
private:
	TArray<ULogData*> GetTreeLogData();

public:	


	ATree* Root;

	int TreeSeed;

	TSubclassOf<ATree> TreeClass;

	FRandomStream NumberStream;

	double StartGenerationTime;

	UProceduralMeshComponent* MaskMesh;
	int nextMeshSectionIndex = 0;

	FProcMeshInfo LogMeshInfo;

	FProcMeshInfo LeavesMeshInfo;


public:
	FJsonObject SerializeObject() override;
	void DeserializeAndLoadObject(FJsonObject ObjectData) override;

	TArray<ULogData*> AllLogData;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FData InitialTreeData;

public:


	// Custom Range functions that simply shortens references to the original RandRange function
	static int RandRange(int Min, int Max, FRandomStream& Stream);
	static float FRandRange(float Min, float Max, FRandomStream& Stream);

};

/*
	Notes:
	Spawning logs take a lot of computational power, so we need to separate generating the full physical tree, and just the mesh without collision
	Full physical tree will be generated when player is close to the chunk containing those trees, or other means
	Mesh will only be generated for trees that are in outer chunks

*/

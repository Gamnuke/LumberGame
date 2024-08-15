// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TreeRoot.generated.h"

class ATree;
struct FRandomStream;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LUMBER_API ATreeRoot : public AActor
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

public:	
	ATree* Root;

	int TreeSeed;

	TSubclassOf<ATree> TreeClass;

	FRandomStream NumberStream;

	double StartGenerationTime;
};

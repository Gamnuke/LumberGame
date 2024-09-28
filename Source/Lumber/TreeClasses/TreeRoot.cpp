// Fill out your copyright notice in the Description page of Project Settings.


#include "TreeRoot.h"
#include "../Tree.h"

// Sets default values for this component's properties
ATreeRoot::ATreeRoot()
{
	
}


// Called when the game starts
void ATreeRoot::BeginPlay()
{
	Super::BeginPlay();
}

/*
Create a root, setup random number stream with seed and start generating tree
*/
void ATreeRoot::GenerateTree() {
	// Sets up stream with given seed
	NumberStream = FRandomStream(TreeSeed);
	
	// Create first branch to start recursively generating
	ATree* NewTree = GetWorld()->SpawnActor<ATree>(TreeClass, GetActorLocation(), FRotator());
	if (NewTree != nullptr) {
		StartGenerationTime = FPlatformTime::Seconds();
		NewTree->TreeRoot = this;
		NewTree->Seed = FMath::Rand();
		NewTree->FirstTree = true;
		NewTree->StartGeneration();
	}
}

/*
Called by first branch to notify that the tree is fully generated and rendered
*/
void ATreeRoot::OnFinishGeneration() {
	double GenerationDuration = FPlatformTime::Seconds() - StartGenerationTime;
	GEngine->AddOnScreenDebugMessage(FMath::Rand(), 20, FColor::Green, FString() + "Took " + FString::SanitizeFloat(GenerationDuration) + "s to make tree");
}


// Called every frame
void ATreeRoot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


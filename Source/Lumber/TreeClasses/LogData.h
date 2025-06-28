// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LogData.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FData {
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FVector LocalLocation;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FVector UpVector = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int BranchHeight;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bPartOfRoot = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bMakeLeaves = false;

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Chance)
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

	// How straight upwards the tree will be (lower values means straighter tree)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float StraightAmount = 0.5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMaterialInterface* LogMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMaterialInterface* LeafMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMaterialInterface* CrossSectionMaterial;
};

UCLASS()
class LUMBER_API ULogData : public UObject
{
	GENERATED_BODY()
public:
	ULogData();

	ULogData *Parent;
	TArray<ULogData*> Children;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FData Data;
};
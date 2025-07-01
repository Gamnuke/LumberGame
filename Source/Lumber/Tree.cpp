// Fill out your copyright notice in the Description page of Project Settings.


#include "Tree.h"
#include "Kismet/KismetSystemLibrary.h"
#include "KismetProceduralMeshLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "TreeMesh.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "PaperSpriteComponent.h"
#include "SceneManagement.h"
#include "Async/Async.h"
#include "Experimental/Async/AwaitableTask.h"
#include "Math/RandomStream.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "LumberGameMode.h"
#include "TreeClasses/TreeRoot.h"

#define GamePriority ENamedThreads::GameThread
#define BackgroundPriority ENamedThreads::AnyBackgroundHiPriTask

// Sets default values
ATree::ATree()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BoxCollision = CreateDefaultSubobject<UBoxComponent>(FName("BoxCollision"));
	SetRootComponent(BoxCollision);
	BoxCollision->SetAngularDamping(1);

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(FName("TreeMeshComponent"));
	Mesh->AttachToComponent(BoxCollision, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Mesh->WeldTo(BoxCollision);
	Mesh->bUseComplexAsSimpleCollision = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetSimulatePhysics(false);

	ParticleSystem = CreateDefaultSubobject<UNiagaraComponent>(FName("ParticleSystem"));
	ParticleSystem->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
}

// Called when the game starts or when spawned
void ATree::BeginPlay()
{
	Super::BeginPlay();
}

FProcMeshInfo ATree::CreateMeshData(bool bBuildLeaves, FData NewLogData, FRandomStream NumberStream, EChunkQuality RenderQuality = EChunkQuality::Low, FVector LocalOrigin = FVector::ZeroVector, FVector UpVector = FVector::UpVector) {
	switch (RenderQuality)
	{
	case Low:
		return GetLowQualityMesh(bBuildLeaves, NewLogData, NumberStream, LocalOrigin, UpVector);
	case High:
		return GetHighQualityMesh(bBuildLeaves, NewLogData, NumberStream, LocalOrigin, UpVector);
		break;
	default:
		return GetHighQualityMesh(bBuildLeaves, NewLogData, NumberStream, LocalOrigin, UpVector);
		break;
	}
}

/* builds geometry for this branch */
void ATree::BuildTreeMesh(bool bBuildLeaves) {

	if (!ThisLogData.bMakeLeaves) {
		// make empty mesh section as placeholder for leaves
		AsyncTask(GamePriority, [this]() {
			Mesh->CreateMeshSection(0, TArray<FVector>(), TArray<int32>(), TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		});
	}
	else {
		FProcMeshInfo NewLeavesMeshInfo = CreateLeavesMeshData(ThisLogData, TreeRoot->NumberStream, FVector::ZeroVector, FVector::UpVector);
		LeafMeshInfo = NewLeavesMeshInfo;

		AsyncTask(GamePriority, [this]() {
			Mesh->CreateMeshSection(
				LeafMeshInfo.Index,
				LeafMeshInfo.Vertices,
				LeafMeshInfo.Triangles,
				LeafMeshInfo.Normals,
				LeafMeshInfo.UVs,
				LeafMeshInfo.Colors,
				LeafMeshInfo.MeshTangents,
				false
			);

		Mesh->SetMaterial(LeafMeshIndex, ThisLogData.LeafMaterial);
		});
	}

	FProcMeshInfo NewMeshInfo = CreateMeshData(bBuildLeaves, ThisLogData, TreeRoot->NumberStream);
	BranchMeshInfo = NewMeshInfo;

	AsyncTask(GamePriority, [this]() {
		Mesh->CreateMeshSection(
			BranchMeshInfo.Index,
			BranchMeshInfo.Vertices,
			BranchMeshInfo.Triangles,
			BranchMeshInfo.Normals,
			BranchMeshInfo.UVs,
			BranchMeshInfo.Colors,
			BranchMeshInfo.MeshTangents,
			false
		);

		Mesh->SetMaterial(LogMeshIndex, ThisLogData.LogMaterial);
	});
}

/* creates geometry at index 0 for this tree */
FProcMeshInfo ATree::CreateLeavesMeshData(FData NewLogData, FRandomStream NumberStream, FVector LocalOrigin = FVector::ZeroVector, FVector UpVector = FVector::UpVector)
{
	// make polygons for the leaves
	TArray<FVector> LeafVertices;
	TArray<int32> LeafTriangles;
	TArray<FVector2D> LeafUVs;
	TArray<FVector> LeafNormals;
	TArray<FProcMeshTangent> LeafTangents;
	float Range = NewLogData.RandLeafThreshold * NewLogData.BranchHeight;

	for (int i = 0; i < NewLogData.NumLeaves; i++) {
		// get a random location for the leaf
		float RandLeafX = ATreeRoot::FRandRange(-Range, Range, NumberStream);
		float RandLeafY = ATreeRoot::FRandRange(-Range, Range, NumberStream);
		float RandLeafZ = ATreeRoot::FRandRange(-2*Range, Range, NumberStream);

		// get a random normal for the leaf's rotation
		float RandLeafNormX = ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream);
		float RandLeafNormY = ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream);
		float RandLeafNormZ = ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream);

		/* get 3 random normals */

		// first normal calculated by random numbers
		TArray<FVector> RandomNormals;
		FVector RandomNormal1 = FVector(RandLeafNormX, RandLeafNormY, RandLeafNormZ).GetSafeNormal();
		RandomNormals.Add(RandomNormal1);

		// second normal calculated by getting vector perpendicular to normal 1 in any direction
		FVector RandomNormal2 = FVector::VectorPlaneProject(
			FVector(ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream), ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream), ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream)),
			RandomNormal1
		).GetSafeNormal();
		RandomNormals.Add(RandomNormal2);

		// get third normal by getting vector perpendicular to BOTH vector 1 and 2, only one solution
		FVector RandomNormal3 = UKismetMathLibrary::RotateAngleAxis(RandomNormal1, 90, RandomNormal2);
		RandomNormals.Add(RandomNormal3);

		// for each position for a leaf, make 3 planes of leaves at perpendicular angles to eachother
		for (int j = 0; j < 3; j++)
		{
			LeafNormals.Add(RandomNormals[j]);

			FVector LeafPoint = FVector(RandLeafX, RandLeafY, NewLogData.BranchHeight + RandLeafZ);
			int leafSizeMult = FMath::RandRange(0.5, 1.5);

			// projects a random point to the plane to get the location of the first point of the leaf vertex
			FVector next_vertex_offset = FVector::VectorPlaneProject(
				FVector(ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream), ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream), ATreeRoot::FRandRange(-1.0f, 1.0f, NumberStream)),
				RandomNormals[j]
			).GetSafeNormal() * NewLogData.LeafSize * leafSizeMult;
			//DrawDebugPoint(GetWorld(), next_vertex_offset, 4, FColor::Green, false, 10, 6);

			TArray<int32> Verts;
			int Angle = 0;
			while (Angle < 360) {
				next_vertex_offset = UKismetMathLibrary::RotateAngleAxis(next_vertex_offset, 90, RandomNormals[j]);

				Verts.Add(LeafVertices.Add(next_vertex_offset + LeafPoint + LocalOrigin));

				if (Angle == 0) {
					LeafUVs.Add(FVector2D(0, 0));
				}
				else if (Angle == 90) {
					LeafUVs.Add(FVector2D(1, 0));
				}
				else if (Angle == 180) {
					LeafUVs.Add(FVector2D(1, 1));
				}
				else if (Angle == 270) {
					LeafUVs.Add(FVector2D(0, 1));
				}
				Angle += 90;
			}

			TArray<int32> Tris;
			UKismetProceduralMeshLibrary::ConvertQuadToTriangles(Tris, Verts[0], Verts[1], Verts[2], Verts[3]);
			LeafTriangles.Append(Tris);
		}
		
	}
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(LeafVertices, LeafTriangles, LeafUVs, LeafNormals, LeafTangents);

	// Store mesh info to this branch
	FProcMeshInfo NewMeshInfo = FProcMeshInfo();
	NewMeshInfo.Index = LeafMeshIndex;
	NewMeshInfo.Vertices = LeafVertices;
	NewMeshInfo.Triangles = LeafTriangles;
	NewMeshInfo.Normals = LeafNormals;
	NewMeshInfo.UVs = LeafUVs;
	NewMeshInfo.Colors = TArray<FColor>();
	NewMeshInfo.MeshTangents = LeafTangents;

	return NewMeshInfo; 
}

FProcMeshInfo ATree::GetHighQualityMesh(bool bBuildLeaves, FData NewLogData, FRandomStream NumberStream, FVector LocalOrigin, FVector UpVector)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	int maxVertInRow = 360 / NewLogData.INCREMENT;

	// for each row of vertices in the branch..
	for (int row = 0; row < NewLogData.ROWS; row++) {
		//FVector next_vertex_offset = FVector(WIDTH + 100 * (1 / ((pow((row + 4) * 0.2, 3)))), 0, 0);
		//FVector next_vertex_offset = FVector(WIDTH * (ROWS - sqrt(row))/ROWS, 0, 0);
		FVector next_vertex_offset = FVector(NewLogData.WIDTH, 0, 0);

		FRotator differenceRot = UpVector.Rotation() - FVector::UpVector.Rotation();
		// place a vertex, then rotate by Angle to place the next vertex
		FVector FirstVertex;
		int Angle = 0;
		while (Angle < 360) {
			FVector newVertex = next_vertex_offset;
			newVertex.Z = row * NewLogData.SECTION_HEIGHT;

			//CollisionVert.Add(newVertex);

			// add randomness to this vertex's location
			float randX = ATreeRoot::FRandRange(-NewLogData.SECTION_HEIGHT / NewLogData.R, NewLogData.SECTION_HEIGHT / NewLogData.R, NumberStream);
			float randY = ATreeRoot::FRandRange(-NewLogData.SECTION_HEIGHT / NewLogData.R, NewLogData.SECTION_HEIGHT / NewLogData.R, NumberStream);
			float randZ = 0;
			newVertex += FVector(randX, randY, randZ);

			// Add rotation offset from args
			FRotator newVertRot = newVertex.Rotation();
			float newVertLength = newVertex.Length();
			newVertex = differenceRot.RotateVector(newVertex);

			// Add location offset from args
			newVertex += LocalOrigin;

			if (Angle == 0) {
				FirstVertex = newVertex;
			}

			UVs.Add(FVector2D(Angle / NewLogData.INCREMENT, row));
			//Normals.Add(-1 * newVertex.GetSafeNormal());
			//FProcMeshTangent Tangent;
			//Tangent.TangentX = newVertex.GetSafeNormal();
			//Tangents.Add(Tangent);

			// add vertex to the array
			Vertices.Add(newVertex);
			//UKismetSystemLibrary::DrawDebugString(GetWorld(), GetActorLocation() + newVertex, FString::FromInt(row * (360 / INCREMENT) + Angle / INCREMENT), (AActor*)0, FLinearColor::White, 60);
			Angle += NewLogData.INCREMENT;
			next_vertex_offset = UKismetMathLibrary::RotateAngleAxis(next_vertex_offset, NewLogData.INCREMENT, FVector(0, 0, 1));

		}

		//Vertices.Add(FirstVertex);
		UVs.Add(FVector2D(maxVertInRow, row));
		//UKismetSystemLibrary::DrawDebugString(GetWorld(), GetActorLocation() + FirstVertex +FVector(0,0,10), FString::FromInt(row * (360 / INCREMENT) + Angle / INCREMENT), (AActor*)0, FLinearColor::Red, 60);
	}
	/*for (int i = 0; i < Vertices.Num(); i++)
	{
		UKismetSystemLibrary::DrawDebugString(GetWorld(), GetActorLocation() + Vertices[i], FString::FromInt(i), (AActor*)0, FLinearColor::White, 60);
	}*/

	// add triangles to connect vertices on bottom base of branch
	for (int i = 0; i < maxVertInRow - 2; i++) {
		Triangles.Add(0);
		Triangles.Add((i + 1));
		Triangles.Add((i + 2));
	}

	// add triangles that connect the vertices from one row to the next row up
	for (int row = 0; row < NewLogData.ROWS - 1; row++) {

		//add triangles for the sides
		for (int i = 0; i < maxVertInRow; i++) {
			int next_i = i + 1;
			if (next_i == maxVertInRow) {
				next_i = 0;
			}
			Triangles.Add((row * (360 / NewLogData.INCREMENT) + i));
			Triangles.Add(((row + 1) * (360 / NewLogData.INCREMENT) + next_i));
			Triangles.Add((row * (360 / NewLogData.INCREMENT) + next_i));

			Triangles.Add((row * (360 / NewLogData.INCREMENT) + i));
			Triangles.Add(((row + 1) * (360 / NewLogData.INCREMENT) + i));
			Triangles.Add(((row + 1) * (360 / NewLogData.INCREMENT) + next_i));
		}
	}

	// idk why but somehow the log already has a top base without this snippet of code :shrug:
	// add triangles to connect vertices on Top base of branch
	for (int i = 0; i < maxVertInRow - 2; i++) {
		Triangles.Add((NewLogData.ROWS - 1) * maxVertInRow + 0);
		Triangles.Add((NewLogData.ROWS - 1) * maxVertInRow + (i + 2));
		Triangles.Add((NewLogData.ROWS - 1) * maxVertInRow + (i + 1));
	}

	// create the mesh for this tree

	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

	// Store mesh info to this branch
	FProcMeshInfo NewMeshInfo = FProcMeshInfo();
	NewMeshInfo.Index = LogMeshIndex;
	NewMeshInfo.Vertices = Vertices;
	NewMeshInfo.Triangles = Triangles;
	NewMeshInfo.Normals = Normals;
	NewMeshInfo.UVs = UVs;
	NewMeshInfo.Colors = TArray<FColor>();
	NewMeshInfo.MeshTangents = Tangents;

	return NewMeshInfo;
}

FProcMeshInfo ATree::GetLowQualityMesh(bool bBuildLeaves, FData NewLogData, FRandomStream NumberStream, FVector LocalOrigin, FVector UpVector)
{
	NewLogData.INCREMENT = 90;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	int maxVertInRow = 360 / NewLogData.INCREMENT;

	// for each row of vertices in the branch..
	for (int row = 0; row < 2; row++) {
		FVector next_vertex_offset = FVector(NewLogData.WIDTH, 0, 0);

		FRotator differenceRot = UpVector.Rotation() - FVector::UpVector.Rotation();
		// place a vertex, then rotate by Angle to place the next vertex
		FVector FirstVertex;
		int Angle = 0;
		while (Angle < 360) {
			FVector newVertex = next_vertex_offset;
			newVertex.Z = row * NewLogData.SECTION_HEIGHT * (NewLogData.ROWS - 1);

			// Add rotation offset from args
			FRotator newVertRot = newVertex.Rotation();
			float newVertLength = newVertex.Length();
			newVertex = differenceRot.RotateVector(newVertex);

			// Add location offset from args
			newVertex += LocalOrigin;

			if (Angle == 0) {
				FirstVertex = newVertex;
			}

			UVs.Add(FVector2D(Angle / NewLogData.INCREMENT, row));

			// add vertex to the array
			Vertices.Add(newVertex);
			//UKismetSystemLibrary::DrawDebugString(GetWorld(), GetActorLocation() + newVertex, FString::FromInt(row * (360 / INCREMENT) + Angle / INCREMENT), (AActor*)0, FLinearColor::White, 60);
			Angle += NewLogData.INCREMENT;
			next_vertex_offset = UKismetMathLibrary::RotateAngleAxis(next_vertex_offset, NewLogData.INCREMENT, FVector(0, 0, 1));
		}

		UVs.Add(FVector2D(maxVertInRow, row));
	}

	// add triangles to connect vertices on bottom base of branch
	for (int i = 0; i < maxVertInRow - 2; i++) {
		Triangles.Add(0);
		Triangles.Add((i + 1));
		Triangles.Add((i + 2));
	}

	// add triangles that connect the vertices from one row to the next row up
	for (int row = 0; row < 1; row++) {

		//add triangles for the sides
		for (int i = 0; i < maxVertInRow; i++) {
			int next_i = i + 1;
			if (next_i == maxVertInRow) {
				next_i = 0;
			}
			Triangles.Add((row * (360 / NewLogData.INCREMENT) + i));
			Triangles.Add(((row + 1) * (360 / NewLogData.INCREMENT) + next_i));
			Triangles.Add((row * (360 / NewLogData.INCREMENT) + next_i));

			Triangles.Add((row * (360 / NewLogData.INCREMENT) + i));
			Triangles.Add(((row + 1) * (360 / NewLogData.INCREMENT) + i));
			Triangles.Add(((row + 1) * (360 / NewLogData.INCREMENT) + next_i));
		}
	}

	// add triangles to connect vertices on Top base of branch
	for (int i = 0; i < maxVertInRow - 2; i++) {
		Triangles.Add((NewLogData.ROWS - 1) * maxVertInRow + 0);
		Triangles.Add((NewLogData.ROWS - 1) * maxVertInRow + (i + 2));
		Triangles.Add((NewLogData.ROWS - 1) * maxVertInRow + (i + 1));
	}

	// create the mesh for this tree
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

	// Store mesh info to this branch
	FProcMeshInfo NewMeshInfo = FProcMeshInfo();
	NewMeshInfo.Index = LogMeshIndex;
	NewMeshInfo.Vertices = Vertices;
	NewMeshInfo.Triangles = Triangles;
	NewMeshInfo.Normals = Normals;
	NewMeshInfo.UVs = UVs;
	NewMeshInfo.Colors = TArray<FColor>();
	NewMeshInfo.MeshTangents = Tangents;

	return NewMeshInfo;
}

/*
Recursively goes through branches' children and generates mesh using the mesh information stored in each branch
*/
void ATree::GenerateMeshRecursive(ATree* BranchToGenerate) {

	TArray<AActor*> TreeChildren;
	BranchToGenerate->GetAttachedActors(TreeChildren);

	for (AActor* ChildActor : TreeChildren) {
		ATree* ChildBranch = Cast<ATree>(ChildActor);
		ChildBranch->GenerateMeshRecursive(ChildBranch);
	}

	BranchToGenerate->BuildTreeMesh(BranchToGenerate->bMakeLeaves);
}

/* cuts the tree */
void ATree::CutTree(FVector CutLocation, UProceduralMeshComponent *ProcMesh, ATree *Tree) {
	FVector BottomLocation = ProcMesh->GetComponentLocation();
	FVector TopLocation = BottomLocation + ProcMesh->GetUpVector() * ThisLogData.BranchHeight;

	// get cut location along line inside branch
	FVector CutCentre = UKismetMathLibrary::FindClosestPointOnLine(CutLocation, BottomLocation, ProcMesh->GetUpVector());
	//DrawDebugPoint(GetWorld(), CutCentre, 10, FColor::MakeRandomColor(), false, 5, 10);
	float TopSegLength = (TopLocation - CutCentre).Size();
	float BottomSegLength = (BottomLocation - CutCentre).Size();

	// get closest existing point to the desired cut location and check if that point is within auto cutting bounds
	int FoundCut = 0;
	float MinimumDistance = FLT_MAX;
	for (int i = 0; i < ExistingCuts.Num(); i++)
	{
		// Get distance in world space
		float Distance = ((GetActorLocation() + ProcMesh->GetUpVector() * ExistingCuts[i].RelativeDistance) - CutCentre).Length();
		if (Distance <= MinimumDistance) {
			MinimumDistance = Distance;
			FoundCut = i;
		}
	}

	GEngine->AddOnScreenDebugMessage(356, 10, FColor::Red, FString::SanitizeFloat(MinimumDistance));
	int NewCutIndex = 0;
	// if the new cut is close enough to an existing cut, 
	// increment number of cuts for that existing cuts
	if (MinimumDistance < ThisLogData.MINCUTLENGTH) {
		ExistingCuts[FoundCut].CutAmount++;
		NewCutIndex = FoundCut;
	}
	else {
		// otherwise check if the cut wont cause logs to be too thin
		if (TopSegLength < ThisLogData.MINCUTLENGTH || BottomSegLength < ThisLogData.MINCUTLENGTH) {
			return;
		}

		// make a new cut at the location in relative space
		FCutData NewData;
		NewData.CutAmount = 0;

		FVector RelativeLocation = CutCentre - GetActorLocation();
		NewData.RelativeDistance = RelativeLocation.Length();

		// check if the relative location is in the bottom or top half of the branch
		if (((GetActorLocation() - GetActorUpVector()) - CutCentre).Length() <
			((GetActorLocation() + GetActorUpVector()) - CutCentre).Length()
			) {

			NewData.RelativeDistance *= -1;
		}

		NewCutIndex = ExistingCuts.Add(NewData);
	}


	// log hasnt been cut enough, so dont cut the wood entirely
	if (ExistingCuts[NewCutIndex].CutAmount < 5) {
		return;
	}

	FVector CutWorldLocation = (GetActorLocation() + GetActorUpVector() * ExistingCuts[NewCutIndex].RelativeDistance);
	TopSegLength = (TopLocation - CutWorldLocation).Size();
	BottomSegLength = (BottomLocation - CutWorldLocation).Size();

	//DrawDebugPoint(GetWorld(), BottomLocation, 10, FColor::MakeRandomColor(), false, 5, 10);
	//DrawDebugPoint(GetWorld(), TopLocation, 10, FColor::MakeRandomColor(), false, 5, 11);

	// get parent
	ATree* Parent = nullptr;
	if (GetAttachParentActor() != nullptr) {
		Parent = Cast<ATree>(GetAttachParentActor());
	}

	// keep track of the branch's children and their locations
	TArray<AActor*> TreeChildren;
	TArray<FVector> ChildrenLoc;
	Tree->GetAttachedActors(TreeChildren);
	GEngine->AddOnScreenDebugMessage(10, 5, FColor::White, FString::FromInt(TreeChildren.Num()));
	for (int i = 0; i < TreeChildren.Num(); i++) {
		ChildrenLoc.Add(TreeChildren[i]->GetActorLocation());
	}

	// slice the mesh at the CutCentre location
	UProceduralMeshComponent* OtherHalf;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		ProcMesh,
		CutWorldLocation,
		-ProcMesh->GetUpVector(),
		true,
		OtherHalf,
		EProcMeshSliceCapOption::CreateNewSectionForCap,
		ThisLogData.CrossSectionMaterial
	);
	//OtherHalf->SetMaterial(RingMeshIndex, CrossSectionMaterial);

	// retain the original rotation for the other half
	// and make the other half vertical for accurate calculations
	FRotator OtherRotation = OtherHalf->GetComponentRotation();
	OtherHalf->SetWorldRotation(FRotator::ZeroRotator);

	// spawn and setup the upper half of the log and alter the bottom half of the log
	ATree* NewTree = GetWorld()->SpawnActor<ATree>(this->GetClass(), CutWorldLocation, OtherRotation);

	NewTree->TreeRoot = TreeRoot;
	NewTree->ThisLogData.BranchHeight = TopSegLength;
	this->ThisLogData.BranchHeight = BottomSegLength;
	NewTree->ThisLogData.WIDTH = this->ThisLogData.WIDTH;

	//----------------------------

	for (int i = 0; i < OtherHalf->GetNumSections(); i++)
	{
		TArray<FVector> VertexArray;
		TArray<FProcMeshVertex> ProcVertexes;
		TArray<uint32> ProcIndexes;
		TArray<int32> Triangles;
		TArray<FVector2D> UVs;
		TArray<FVector> Normals;
		TArray<FProcMeshTangent> Tangents;

		// copy over the mesh data of the other half of the log to a new tree object
		if (OtherHalf->GetProcMeshSection(i) != nullptr) {
			ProcVertexes = OtherHalf->GetProcMeshSection(i)->ProcVertexBuffer;
			ProcIndexes = OtherHalf->GetProcMeshSection(i)->ProcIndexBuffer;
		}
		for (int ii = 0; ii < ProcVertexes.Num(); ii++) {
			VertexArray.Add(ProcVertexes[ii].Position - FVector::UpVector * BottomSegLength);
			UVs.Add(ProcVertexes[ii].UV0);
			//UKismetSystemLibrary::DrawDebugString(GetWorld(), CutCentre + ProcVertexes[ii].Position, FString::FromInt(ii), (AActor*)0, FLinearColor::White, 60);
		}
		for (int ii = 0; ii < ProcIndexes.Num(); ii++) {
			Triangles.Add(ProcIndexes[ii]);
		}

		// create the mesh for the other half
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(VertexArray, Triangles, UVs, Normals, Tangents);
		NewTree->Mesh->CreateMeshSection(i + 1, VertexArray, Triangles, Normals, UVs, TArray<FColor>(), Tangents, false);
		NewTree->Mesh->SetMaterial(i + 1, OtherHalf->GetMaterial(i));
	}
	

	//----------------------------

	SetupTree(this, BottomSegLength, ThisLogData.WIDTH);
	SetActorLocation(CutWorldLocation - GetActorUpVector() * BottomSegLength/2);

	SetupTree(NewTree, TopSegLength, ThisLogData.WIDTH);
	NewTree->SetActorLocation(CutWorldLocation + NewTree->GetActorUpVector() * TopSegLength/2);
	NewTree->BoxCollision->SetSimulatePhysics(true);

	// if the bottom part of the tree is connected to the root, disable physics for it
	if (bPartOfRoot) {
		this->BoxCollision->SetSimulatePhysics(false);
	}

	if (ThisLogData.BranchHeight < ThisLogData.WIDTH * 3) {
		//this->DetachRootComponentFromParent(true);
	}

	// attach the children to the top half of the tree
	for (int i = 0; i < TreeChildren.Num(); i++) {
		ATree* Child = Cast<ATree>(TreeChildren[i]);
		//Child->AttachToActor(NewTree, FAttachmentTransformRules::KeepWorldTransform);
		Child->BoxCollision->WeldTo(NewTree->BoxCollision);
		Child->SetActorLocation(ChildrenLoc[i]);
	}

	// weld the children of the cut log to the root
	WeldChildrenRecursive(NewTree, NewTree);
	RemoveLeavesRecursive(NewTree, NewTree);
	NewTree->bPartOfRoot = false;

	OtherHalf->DestroyComponent();
	ExistingCuts.Empty();
	//OtherHalf->WeldTo(NewTree->RootComponent);
}

/* iterates through every branch*/
void ATree::WeldChildrenRecursive(ATree* Root, ATree* NextTree) {
	TArray<AActor*> TreeChildren;
	NextTree->GetAttachedActors(TreeChildren);
	for (int i = 0; i < TreeChildren.Num(); i++) {
		ATree* Child = Cast<ATree>(TreeChildren[i]);
		Child->bPartOfRoot = false;
		Child->BoxCollision->SetNotifyRigidBodyCollision(true);
		Child->BoxCollision->RecreatePhysicsState();
		WeldChildrenRecursive(Root, Child);
	}
}

/* recurses through children connected to the root branch and removes the leaves */
void ATree::RemoveLeavesRecursive(ATree* Root, ATree* NextTree) {
	TArray<AActor*> TreeChildren;
	NextTree->GetAttachedActors(TreeChildren);
	NextTree->Mesh->ClearMeshSection(LeafMeshIndex);

	for (int i = 0; i < TreeChildren.Num(); i++) {
		ATree* Child = Cast<ATree>(TreeChildren[i]);


		RemoveLeavesRecursive(Root, Child);
	}
}

/* sets up collision and data for a given tree */
void ATree::SetupTree(ATree* Tree, int NewBranchHeight, int NewBranchWidth) {
	Tree->ThisLogData.BranchHeight = NewBranchHeight;

	// setup the capsule component
	//Tree->BoxCollision->SetCapsuleHalfHeight(NewBranchHeight / 2);
	//Tree->BoxCollision->SetCapsuleRadius(NewBranchWidth);

	Tree->BoxCollision->SetBoxExtent(FVector(NewBranchWidth, NewBranchWidth, NewBranchHeight/2));
	Tree->BoxCollision->SetMassOverrideInKg(NAME_None, (NewBranchWidth * NewBranchWidth * NewBranchHeight/2)/10000, true);
	Tree->BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Tree->BoxCollision->SetCollisionProfileName(FName("BlockAll"));

	//if (!Tree->FirstTree) {
	//	Tree->CapsuleComponent->SetSimulatePhysics(true);
	//}

	// the mesh starts at in the middle of the capsule, so offset it to the right position
	//Tree->Mesh->SetRelativeLocation(FVector(0, 0, -NewBranchHeight / 2));
	Tree->Mesh->SetWorldLocation(Tree->BoxCollision->GetComponentLocation() - Tree->BoxCollision->GetUpVector() * NewBranchHeight / 2);
	Tree->Mesh->bUseComplexAsSimpleCollision = false;
	Tree->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tree->Mesh->SetSimulatePhysics(false);
}

// Called every frame
void ATree::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	for (int i = 0; i < ExistingCuts.Num(); i++)
	{
		DrawDebugPoint(GetWorld(), GetActorLocation() + GetActorUpVector() * ExistingCuts[i].RelativeDistance, 10, FColor::Red, false, DeltaTime * 2, 11);
	}
}
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

	// Start generating tree from this branch if its the first branch
	if (FirstTree) {
		StartGeneration();
	}
}

/*
Called on the first branch
*/
void ATree::StartGeneration() {

	// Setup variables for this branch
	BoxCollision->SetSimulatePhysics(false);
	ROWS *= TrunkHeightMultiplier;
	BranchHeight = (ROWS - 1) * SECTION_HEIGHT;

	bPartOfRoot = true;
	SetActorLocation(GetActorLocation() + GetActorUpVector() * BranchHeight / 2);

	// Start generating the branch actors (spawning and collision only)
	BuildTreeMeshRecursive(4, 0, this, true);

	// Once all branches are spawned, recursively generate the meshes on all branches on a seperate thread
	AsyncTask(BackgroundPriority, [this]() {
		GenerateMeshRecursive(this);
		TreeRoot->OnFinishGeneration();
	});

	SetupTree(this, BranchHeight, WIDTH);
}


/* main function used to recursively build the tree ground up */
void ATree::BuildTreeMeshRecursive(int MaxDepth, int CurrentDepth, ATree *Parent, bool Extension) {
	// stop building the tree if this branch exceeds the max depth
	if (CurrentDepth > MaxDepth) { return; }

	// get random number for how many branches branch off from the parent branch 
	int iRange = RandRange(BranchNumMin, BranchNumMax - BranchNumMax * (CurrentDepth / MaxDepth), TreeRoot->NumberStream);
	if (Extension) {
		iRange = 1;
	}

	// for each new branch...
	for (int i = 0; i < iRange; i++) {

		// chance to spawn a branch at side of log instead of end of log
		bool SideStem = RandRange(0, 100, TreeRoot->NumberStream) <= SideStemChance;
		float StemAmount = RandRange(0.0f, 1.0f, TreeRoot->NumberStream);
		if (!SideStem || CurrentDepth <= 2) {
			StemAmount = 1;
		}

		// random angle for new branch
		float Randomness = (60 - (60 * (CurrentDepth / MaxDepth)) * 0.7) * (CurrentDepth * 2/MaxDepth + StraightAmount);
		float RandPitch = FRandRange(-Randomness, Randomness, TreeRoot->NumberStream);
		float RandYaw = FRandRange(-Randomness, Randomness, TreeRoot->NumberStream);
		float RandRow = FRandRange(-Randomness, Randomness, TreeRoot->NumberStream);
		FVector NewTreeLoc = Parent->GetActorLocation() + Parent->GetActorUpVector() * (Parent->BranchHeight / 2) * StemAmount;
		FRotator NewTreeRot = Parent->GetActorRotation() + FRotator(RandPitch, RandYaw, RandRow);
		/*NewTreeRot = FRotator(
			FMath::ClampAngle(NewTreeRot.Pitch, -90, 90),
			FMath::ClampAngle(NewTreeRot.Yaw, -90, 90),
			FMath::ClampAngle(NewTreeRot.Roll, -90, 90)
		);*/

		// spawns new branch and setups data

		ATree* NewTree = GetWorld()->SpawnActor<ATree>(
			this->GetClass(),
			NewTreeLoc,
			FRotator()
		);

		//NewTree->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
		NewTree->WIDTH = FMath::Clamp(NewTree->WIDTH * (MaxDepth - CurrentDepth) / MaxDepth, NewTree->WIDTH / 4, NewTree->WIDTH);
		NewTree->ROWS = FRandRange(NewTree->ROWS / (CurrentDepth + 1), NewTree->ROWS, TreeRoot->NumberStream);
		NewTree->BranchHeight = (NewTree->ROWS - 1) * NewTree->SECTION_HEIGHT;
		
		NewTree->TreeRoot = TreeRoot;
		NewTree->SetupTree(NewTree, NewTree->BranchHeight, NewTree->WIDTH);
		NewTree->SetActorRotation(NewTreeRot);
		NewTree->AddActorWorldOffset(NewTree->GetActorUpVector() * NewTree->BranchHeight / 2);

		NewTree->BoxCollision->SetSimulatePhysics(false);
		NewTree->BoxCollision->WeldTo(Parent->BoxCollision);
		NewTree->bPartOfRoot = true;

		//chance to extend this branch, recursing with the same depth
		bool ExtendThis = (RandRange(0, 100, TreeRoot->NumberStream) <= ExtendChance);
		if (CurrentDepth == MaxDepth && !ExtendThis) {
			ParticleSystem->SetWorldLocation(NewTreeLoc);
			ParticleSystem->SetAutoActivate(true);
			ParticleSystem->Activate(true);
			ParticleSystem->ActivateSystem();
			//ParticleSystem->ResetSystem();

			NewTree->bMakeLeaves = true;

		}
		else {
			NewTree->bMakeLeaves = false;

		}
		if (ExtendThis) {
			//Cast<ALumberGameMode>(GetWorld()->GetAuthGameMode())->TaskQueue.Enqueue();
			BuildTreeMeshRecursive(MaxDepth, CurrentDepth, NewTree, true);
		}
		else {
			//Cast<ALumberGameMode>(GetWorld()->GetAuthGameMode())->TaskQueue.Enqueue();
			BuildTreeMeshRecursive(MaxDepth, CurrentDepth + 1, NewTree, false);
		}

	}
}

/* builds geometry for this branch */
void ATree::BuildTreeMesh(bool bBuildLeaves) {

	if (bBuildLeaves == false) {
		// make empty mesh section as placeholder for leaves
		AsyncTask(GamePriority, [this]() {
			Mesh->CreateMeshSection(0, TArray<FVector>(), TArray<int32>(), TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		});
	}
	else {
		MakeLeaves();
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;

	// for each row of vertices in the branch..
	for (int row = 0; row < ROWS; row++) {
		//FVector next_vertex_offset = FVector(WIDTH + 100 * (1 / ((pow((row + 4) * 0.2, 3)))), 0, 0);
		//FVector next_vertex_offset = FVector(WIDTH * (ROWS - sqrt(row))/ROWS, 0, 0);
		FVector next_vertex_offset = FVector(WIDTH, 0, 0);

		// place a vertex, then rotate by Angle to place the next vertex

		FVector FirstVertex;
		int Angle = 0;
		while (Angle < 360) {
			next_vertex_offset = UKismetMathLibrary::RotateAngleAxis(next_vertex_offset, INCREMENT, FVector(0, 0, 1));
			FVector newVertex = next_vertex_offset;
			newVertex.Z = row * SECTION_HEIGHT;

			//CollisionVert.Add(newVertex);

			// add randomness to this vertex's location
			float randX = FRandRange(-SECTION_HEIGHT / R, SECTION_HEIGHT / R, TreeRoot->NumberStream);
			float randY = FRandRange(-SECTION_HEIGHT / R, SECTION_HEIGHT / R, TreeRoot->NumberStream);
			float randZ = 0;
			newVertex += FVector(randX, randY, randZ);
			if (Angle == 0) {
				FirstVertex = newVertex;
			}
			
			UVs.Add(FVector2D(Angle / INCREMENT, row));
			//Normals.Add(-1 * newVertex.GetSafeNormal());
			//FProcMeshTangent Tangent;
			//Tangent.TangentX = newVertex.GetSafeNormal();
			//Tangents.Add(Tangent);

			// add vertex to the array
			Vertices.Add(newVertex);
			//UKismetSystemLibrary::DrawDebugString(GetWorld(), GetActorLocation() + newVertex, FString::FromInt(row * (360 / INCREMENT) + Angle / INCREMENT), (AActor*)0, FLinearColor::White, 60);
			Angle += INCREMENT;
		}

		Vertices.Add(FirstVertex);
		UVs.Add(FVector2D(360/INCREMENT, row));
		//UKismetSystemLibrary::DrawDebugString(GetWorld(), GetActorLocation() + FirstVertex +FVector(0,0,10), FString::FromInt(row * (360 / INCREMENT) + Angle / INCREMENT), (AActor*)0, FLinearColor::Red, 60);
	}
	/*for (int i = 0; i < Vertices.Num(); i++)
	{
		UKismetSystemLibrary::DrawDebugString(GetWorld(), GetActorLocation() + Vertices[i], FString::FromInt(i), (AActor*)0, FLinearColor::White, 60);
	}*/

	// add triangles to connect vertices on bottom base of branch
	for (int i = 0; i < 360 / INCREMENT + 1; i++) {
		Triangles.Add(0);
		Triangles.Add((i + 1));
		Triangles.Add((i + 2));
	}

	// add triangles that connect the vertices from one row to the next row up
	for (int row = 0; row < ROWS; row++) {
		//add triangles for the sides
		for (int i = 0; i < 360 / INCREMENT + 1; i++) {
			int next_i = i + 1;
			if (next_i == 360 / INCREMENT + 1) {
				next_i = 0;
			}
			Triangles.Add((row * (360 / INCREMENT + 1) + i));
			Triangles.Add(((row + 1) * (360 / INCREMENT + 1) + next_i));
			Triangles.Add((row * (360 / INCREMENT + 1) + next_i));

			Triangles.Add((row * (360 / INCREMENT + 1) + i));
			Triangles.Add(((row + 1) * (360 / INCREMENT + 1) + i));
			Triangles.Add(((row + 1) * (360 / INCREMENT + 1) + next_i));
		}
	}

	// idk why but somehow the log already has a top base without this snippet of code :shrug:
	// add triangles to connect vertices on Top base of branch
	/*for (int i = 0; i < 360 / INCREMENT + 1; i++) {
		Triangles.Add(ROWS * 360 / INCREMENT + 1 + (i + 2));
		Triangles.Add(ROWS * 360 / INCREMENT + 1 + (i + 1));
		Triangles.Add(ROWS * 360 / INCREMENT + 1 + 0);
	}*/

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

		Mesh->SetMaterial(LogMeshIndex, LogMaterial);
	});
}

/* creates geometry at index 0 for this tree */
void ATree::MakeLeaves()
{

	// make polygons for the leaves
	TArray<FVector> LeafVertices;
	TArray<int32> LeafTriangles;
	TArray<FVector2D> LeafUVs;
	TArray<FVector> LeafNormals;
	TArray<FProcMeshTangent> LeafTangents;
	float Range = RandLeafThreshold * BranchHeight;

	for (int i = 0; i < NumLeaves; i++) {
		// get a random location for the leaf
		float RandLeafX = FRandRange(-Range, Range, TreeRoot->NumberStream);
		float RandLeafY = FRandRange(-Range, Range, TreeRoot->NumberStream);
		float RandLeafZ = FRandRange(-2*Range, Range, TreeRoot->NumberStream);

		// get a random normal for the leaf's rotation
		float RandLeafNormX = FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream);
		float RandLeafNormY = FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream);
		float RandLeafNormZ = FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream);

		/* get 3 random normals */

		// first normal calculated by random numbers
		TArray<FVector> RandomNormals;
		FVector RandomNormal1 = FVector(RandLeafNormX, RandLeafNormY, RandLeafNormZ).GetSafeNormal();
		RandomNormals.Add(RandomNormal1);

		// second normal calculated by getting vector perpendicular to normal 1 in any direction
		FVector RandomNormal2 = FVector::VectorPlaneProject(
			FVector(FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream), FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream), FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream)),
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

			FVector LeafPoint = FVector(RandLeafX, RandLeafY, BranchHeight + RandLeafZ);
			int leafSizeMult = FMath::RandRange(0.5, 1.5);

			// projects a random point to the plane to get the location of the first point of the leaf vertex
			FVector next_vertex_offset = FVector::VectorPlaneProject(
				FVector(FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream), FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream), FRandRange(-1.0f, 1.0f, TreeRoot->NumberStream)),
				RandomNormals[j]
			).GetSafeNormal() * LeafSize * leafSizeMult;
			//DrawDebugPoint(GetWorld(), next_vertex_offset, 4, FColor::Green, false, 10, 6);

			TArray<int32> Verts;
			int Angle = 0;
			while (Angle < 360) {
				next_vertex_offset = UKismetMathLibrary::RotateAngleAxis(next_vertex_offset, 90, RandomNormals[j]);

				Verts.Add(LeafVertices.Add(next_vertex_offset + LeafPoint));

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
	LeafMeshInfo = NewMeshInfo;

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

		Mesh->SetMaterial(LeafMeshIndex, LeafMaterial);
	});
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

	//// Generate branch mesh
	//Mesh->CreateMeshSection(
	//	BranchMeshInfo.Index,
	//	BranchMeshInfo.Vertices,
	//	BranchMeshInfo.Triangles,
	//	BranchMeshInfo.Normals,
	//	BranchMeshInfo.UVs,
	//	BranchMeshInfo.Colors,
	//	BranchMeshInfo.MeshTangents,
	//	false
	//);

	//Mesh->CreateMeshSection(
	//	LeafMeshInfo.Index,
	//	LeafMeshInfo.Vertices,
	//	LeafMeshInfo.Triangles,
	//	LeafMeshInfo.Normals,
	//	LeafMeshInfo.UVs,
	//	LeafMeshInfo.Colors,
	//	LeafMeshInfo.MeshTangents,
	//	false
	//);

	//Mesh->SetMaterial(LogMeshIndex, LogMaterial);
	//Mesh->SetMaterial(LeafMeshIndex, LeafMaterial);

}

/* cuts the tree */
void ATree::CutTree(FVector CutLocation, UProceduralMeshComponent *ProcMesh, ATree *Tree) {
	FVector BottomLocation = ProcMesh->GetComponentLocation();
	FVector TopLocation = BottomLocation + ProcMesh->GetUpVector() * BranchHeight;

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
	if (MinimumDistance < MINCUTLENGTH) {
		ExistingCuts[FoundCut].CutAmount++;
		NewCutIndex = FoundCut;
	}
	else {
		// otherwise check if the cut wont cause logs to be too thin
		if (TopSegLength < MINCUTLENGTH || BottomSegLength < MINCUTLENGTH) {
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
		CrossSectionMaterial
	);
	//OtherHalf->SetMaterial(RingMeshIndex, CrossSectionMaterial);

	// retain the original rotation for the other half
	// and make the other half vertical for accurate calculations
	FRotator OtherRotation = OtherHalf->GetComponentRotation();
	OtherHalf->SetWorldRotation(FRotator::ZeroRotator);

	// spawn and setup the upper half of the log and alter the bottom half of the log
	ATree* NewTree = GetWorld()->SpawnActor<ATree>(this->GetClass(), CutWorldLocation, OtherRotation);

	NewTree->TreeRoot = TreeRoot;
	NewTree->BranchHeight = TopSegLength;
	this->BranchHeight = BottomSegLength;
	NewTree->WIDTH = this->WIDTH;

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

	SetupTree(this, BottomSegLength, WIDTH);
	SetActorLocation(CutWorldLocation - GetActorUpVector() * BottomSegLength/2);

	SetupTree(NewTree, TopSegLength, WIDTH);
	NewTree->SetActorLocation(CutWorldLocation + NewTree->GetActorUpVector() * TopSegLength/2);
	NewTree->BoxCollision->SetSimulatePhysics(true);

	// if the bottom part of the tree is connected to the root, disable physics for it
	if (bPartOfRoot) {
		this->BoxCollision->SetSimulatePhysics(false);
	}

	if (BranchHeight < WIDTH * 3) {
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
	Tree->BranchHeight = NewBranchHeight;

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

int ATree::RandRange(int Min, int Max, FRandomStream &Stream) {
	int NewNum = UKismetMathLibrary::RandomIntegerInRangeFromStream(Stream, Min, Max);
	//GEngine->AddOnScreenDebugMessage(FMath::Rand(), 10, FColor::Green, FString::FromInt(NewNum));
	return NewNum;
	//return RandomStream.RandRange(Min, Max);
}

float ATree::FRandRange(float Min, float Max, FRandomStream& Stream) {
	float NewNum = UKismetMathLibrary::RandomFloatInRangeFromStream(Stream, Min, Max);
	//GEngine->AddOnScreenDebugMessage(FMath::Rand(), 10, FColor::Green, FString::SanitizeFloat(NewNum));
	return NewNum;
	//return RandomStream.FRandRange(Min, Max);
}



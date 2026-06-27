// Fill out your copyright notice in the Description page of Project Settings.

#include "HDMapVisualizer.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"

AHDMapVisualizer::AHDMapVisualizer()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 기본값 초기화
	PointSpacing = 200.f; // 2m 간격 화살표 배치
	InstanceScale = FVector(0.5f, 0.5f, 0.5f);
}

void AHDMapVisualizer::BeginPlay()
{
	Super::BeginPlay();
}

void AHDMapVisualizer::ImportHDMapData()
{
	if (SourceGISFolder.Path.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapVisualizer] SourceGISFolder is empty. Please specify a GIS data folder."));
		return;
	}

	FString ProjectDir = FPaths::ProjectDir();
	FString PythonScriptPath = FPaths::Combine(ProjectDir, TEXT("Python"), TEXT("hdmap_parser.py"));
	FString AbsoluteGISPath = SourceGISFolder.Path;

	if (FPaths::IsRelative(AbsoluteGISPath))
	{
		AbsoluteGISPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, AbsoluteGISPath));
	}

	FString TargetJSONPath = FPaths::Combine(ProjectDir, TEXT("Python"), TEXT("HDMapData.json"));

	// 백그라운드 프로세스 명령 파라미터 구성
	FString Args = FString::Printf(TEXT("\"%s\" --input \"%s\" --output \"%s\" --prefix \"Cliped_\" --scale 1.0"),
		*PythonScriptPath, *AbsoluteGISPath, *TargetJSONPath);

	UE_LOG(LogTemp, Log, TEXT("[AHDMapVisualizer] Running parser: py %s"), *Args);

	int32 ReturnCode = -1;
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	FProcHandle Proc = FPlatformProcess::CreateProc(
		TEXT("py"),
		*Args,
		true, // bLaunchDetached
		true, // bLaunchHidden
		true, // bLaunchReallyHidden
		nullptr, // OutProcessID
		0,
		nullptr,
		WritePipe,
		ReadPipe
	);

	if (Proc.IsValid())
	{
		FPlatformProcess::Sleep(0.1f);
		int32 Watchdog = 0;
		while (FPlatformProcess::IsProcRunning(Proc) && Watchdog < 150)
		{
			FPlatformProcess::Sleep(0.1f);
			Watchdog++;
		}
		FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
		FPlatformProcess::CloseProc(Proc);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapVisualizer] Failed to launch with 'py'. Retrying with 'python'..."));
		Proc = FPlatformProcess::CreateProc(
			TEXT("python"),
			*Args,
			true, true, true, nullptr, 0, nullptr, WritePipe, ReadPipe
		);
		if (Proc.IsValid())
		{
			FPlatformProcess::Sleep(0.1f);
			int32 Watchdog = 0;
			while (FPlatformProcess::IsProcRunning(Proc) && Watchdog < 150)
			{
				FPlatformProcess::Sleep(0.1f);
				Watchdog++;
			}
			FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
			FPlatformProcess::CloseProc(Proc);
		}
	}

	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	if (ReturnCode != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapVisualizer] Parser failed to execute or return code was non-zero (%d)."), ReturnCode);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[AHDMapVisualizer] GIS conversion succeeded. Loading JSON to DataTables..."));

	// 데이터테이블 에셋과 각 개별 JSON 파일 매핑
	TMap<UDataTable*, FString> TableToJSONMap;
	TableToJSONMap.Add(DT_A1_Node, TEXT("HDMap_A1.json"));
	TableToJSONMap.Add(DT_A2_Link, TEXT("HDMap_A2.json"));
	TableToJSONMap.Add(DT_A3_Driveway, TEXT("HDMap_A3.json"));
	TableToJSONMap.Add(DT_A4_Subsidiary, TEXT("HDMap_A4.json"));
	TableToJSONMap.Add(DT_Sidewalk, TEXT("HDMap_Sidewalk.json"));
	TableToJSONMap.Add(DT_B2_Lane, TEXT("HDMap_B2.json"));
	TableToJSONMap.Add(DT_B3_Mark, TEXT("HDMap_B3.json"));
	TableToJSONMap.Add(DT_C1_Light, TEXT("HDMap_C1.json"));
	TableToJSONMap.Add(DT_C3_Protection, TEXT("HDMap_C3.json"));
	TableToJSONMap.Add(DT_C4_SpeedBump, TEXT("HDMap_C4.json"));
	TableToJSONMap.Add(DT_C5_Barrier, TEXT("HDMap_C5.json"));
	TableToJSONMap.Add(DT_C6_Post, TEXT("HDMap_C6.json"));
	TableToJSONMap.Add(DT_Building, TEXT("HDMapBuilding.json"));
	TableToJSONMap.Add(HDMapDataTable, TEXT("HDMapData.json"));

	int32 ReimportedCount = 0;
	for (auto& Elem : TableToJSONMap)
	{
		UDataTable* Table = Elem.Key;
		FString JsonFileName = Elem.Value;

		if (!Table) continue;

		FString FullJsonPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectDir, TEXT("Python"), JsonFileName));

		// 파일로부터 JSON 문자열 로드
		FString JsonContent;
		if (FFileHelper::LoadFileToString(JsonContent, *FullJsonPath))
		{
			// CreateTableFromJSONString API를 사용하여 에디터 모듈 없이 즉각 데이터 파싱 및 이식
			TArray<FString> Errors = Table->CreateTableFromJSONString(JsonContent);
			if (Errors.Num() > 0)
			{
				for (const FString& Err : Errors)
				{
					UE_LOG(LogTemp, Warning, TEXT("[AHDMapVisualizer] JSON Parser Error in %s: %s"), *JsonFileName, *Err);
				}
			}
			else
			{
				Table->MarkPackageDirty();
				ReimportedCount++;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AHDMapVisualizer] Failed to read JSON file: %s"), *FullJsonPath);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[AHDMapVisualizer] Success! Updated %d DataTables from JSON data."), ReimportedCount);
}

void AHDMapVisualizer::VisualizeHDMap()
{
	ClearVisualization();

	UStaticMesh* FallbackLinkMesh = LinkStaticMesh;
	if (!FallbackLinkMesh)
	{
		FallbackLinkMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")));
	}

	UStaticMesh* FallbackLaneMesh = LaneStaticMesh;
	if (!FallbackLaneMesh)
	{
		FallbackLaneMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	}

	UStaticMesh* FallbackSphereMesh = SphereStaticMesh;
	if (!FallbackSphereMesh)
	{
		FallbackSphereMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	}

	UMaterialInterface* FallbackMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")));

	// 5색 매칭 에셋 준비
	UMaterialInterface* RedMat = LinkRedMaterial ? LinkRedMaterial : FallbackMaterial;
	UMaterialInterface* GreenMat = LaneGreenMaterial ? LaneGreenMaterial : FallbackMaterial;
	UMaterialInterface* PinkMat = CurbPinkMaterial ? CurbPinkMaterial : FallbackMaterial;
	UMaterialInterface* YellowMat = SidewalkYellowMaterial ? SidewalkYellowMaterial : FallbackMaterial;
	UMaterialInterface* BlueMat = MarkBlueMaterial ? MarkBlueMaterial : FallbackMaterial;

	// 레이어별 인스턴스 핀(Pin) 드로잉 도우미 람다 함수 정의
	auto DrawLayerHelper = [&](UDataTable* DataTable, UStaticMesh* HeadMesh, UStaticMesh* BodyMesh, UStaticMesh* JointMesh, UMaterialInterface* Mat, auto RowTypePtr)
	{
		if (!DataTable) return;

		using RowType = typename TRemovePointer<decltype(RowTypePtr)>::Type;
		TArray<RowType*> Rows;
		DataTable->GetAllRows<RowType>(TEXT("HDMapVisualizer"), Rows);

		for (RowType* Row : Rows)
		{
			if (!Row) continue;
			if (Row->ID.Equals(TEXT("ORIGIN"))) continue;
			if (Row->Points.Num() == 0) continue;

			FString ID = Row->ID;

			// 1. 머리 컴포넌트 생성 (피라미드)
			UInstancedStaticMeshComponent* ISMCompHead = nullptr;
			if (HeadMesh && Row->Points.Num() >= 2)
			{
				FString CompName = FString::Printf(TEXT("ISM_Head_%s"), *ID);
				ISMCompHead = NewObject<UInstancedStaticMeshComponent>(this, UInstancedStaticMeshComponent::StaticClass(), FName(*CompName));
				if (ISMCompHead)
				{
					ISMCompHead->SetupAttachment(RootComponent);
					ISMCompHead->RegisterComponent();
					ISMCompHead->SetMobility(EComponentMobility::Static);
					ISMCompHead->SetStaticMesh(HeadMesh);
					ISMCompHead->SetMaterial(0, Mat);
					CreatedComponents.Add(FString::Printf(TEXT("Head_%s"), *ID), ISMCompHead);
				}
			}

			// 2. 몸통 컴포넌트 생성 (큐브)
			UInstancedStaticMeshComponent* ISMCompBody = nullptr;
			if (BodyMesh && Row->Points.Num() >= 2)
			{
				FString CompName = FString::Printf(TEXT("ISM_Body_%s"), *ID);
				ISMCompBody = NewObject<UInstancedStaticMeshComponent>(this, UInstancedStaticMeshComponent::StaticClass(), FName(*CompName));
				if (ISMCompBody)
				{
					ISMCompBody->SetupAttachment(RootComponent);
					ISMCompBody->RegisterComponent();
					ISMCompBody->SetMobility(EComponentMobility::Static);
					ISMCompBody->SetStaticMesh(BodyMesh);
					ISMCompBody->SetMaterial(0, Mat);
					CreatedComponents.Add(FString::Printf(TEXT("Body_%s"), *ID), ISMCompBody);
				}
			}

			// 3. 마디 Joint 컴포넌트 생성 (구체)
			UInstancedStaticMeshComponent* ISMCompJoint = nullptr;
			if (JointMesh)
			{
				FString CompName = FString::Printf(TEXT("ISM_Joint_%s"), *ID);
				ISMCompJoint = NewObject<UInstancedStaticMeshComponent>(this, UInstancedStaticMeshComponent::StaticClass(), FName(*CompName));
				if (ISMCompJoint)
				{
					ISMCompJoint->SetupAttachment(RootComponent);
					ISMCompJoint->RegisterComponent();
					ISMCompJoint->SetMobility(EComponentMobility::Static);
					ISMCompJoint->SetStaticMesh(JointMesh);
					ISMCompJoint->SetMaterial(0, Mat);
					CreatedComponents.Add(FString::Printf(TEXT("Joint_%s"), *ID), ISMCompJoint);
				}
			}

			DrawLineSegment(ISMCompHead, ISMCompBody, ISMCompJoint, Row->Points);
		}
	};

	// 1. DT_A2_Link ➡️ 빨강
	DrawLayerHelper(DT_A2_Link, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, RedMat, (FHDMapA2LinkRow*)nullptr);

	// 2. DT_B2_Lane ➡️ 초록
	DrawLayerHelper(DT_B2_Lane, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, GreenMat, (FHDMapB2LaneRow*)nullptr);

	// 3. DT_C3_Protection (연석/가드레일 등) ➡️ 분홍
	DrawLayerHelper(DT_C3_Protection, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, PinkMat, (FHDMapC3ProtectionRow*)nullptr);

	// 4. DT_A4_Subsidiary (보도) ➡️ 노랑
	DrawLayerHelper(DT_A4_Subsidiary, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, YellowMat, (FHDMapA4SubsidiaryRow*)nullptr);

	// 4-2. DT_Sidewalk (인도 병합) ➡️ 노랑
	DrawLayerHelper(DT_Sidewalk, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, YellowMat, (FHDMapSidewalkRow*)nullptr);

	// 5. DT_B3_Mark (노면 기호) ➡️ 파랑
	DrawLayerHelper(DT_B3_Mark, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, BlueMat, (FHDMapB3MarkRow*)nullptr);

	// 6. DT_C4_SpeedBump (과속방지턱) ➡️ 파랑
	DrawLayerHelper(DT_C4_SpeedBump, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, BlueMat, (FHDMapC4SpeedBumpRow*)nullptr);

	// 6-1. DT_C5_Barrier (높이제한장해물) ➡️ 분홍
	DrawLayerHelper(DT_C5_Barrier, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, PinkMat, (FHDMapC5BarrierRow*)nullptr);

	// 6-2. DT_C6_Post (기둥) ➡️ 분홍
	DrawLayerHelper(DT_C6_Post, FallbackLinkMesh, FallbackLaneMesh, FallbackSphereMesh, PinkMat, (FHDMapC6PostRow*)nullptr);

	// 7. DT_A1_Node (정점 노드, 화살표 없이 구체 Joint만 굵게 그림)
	if (DT_A1_Node)
	{
		TArray<FHDMapA1NodeRow*> Rows;
		DT_A1_Node->GetAllRows<FHDMapA1NodeRow>(TEXT("HDMapVisualizer_Node"), Rows);
		for (FHDMapA1NodeRow* Row : Rows)
		{
			if (!Row || Row->ID.Equals(TEXT("ORIGIN")) || Row->Points.Num() == 0) continue;

			FString ID = Row->ID;
			FString CompName = FString::Printf(TEXT("ISM_Node_%s"), *ID);
			UInstancedStaticMeshComponent* ISMCompNode = NewObject<UInstancedStaticMeshComponent>(this, UInstancedStaticMeshComponent::StaticClass(), FName(*CompName));
			if (ISMCompNode)
			{
				ISMCompNode->SetupAttachment(RootComponent);
				ISMCompNode->RegisterComponent();
				ISMCompNode->SetMobility(EComponentMobility::Static);
				ISMCompNode->SetStaticMesh(FallbackSphereMesh);
				ISMCompNode->SetMaterial(0, YellowMat);

				FTransform NodeTransform;
				NodeTransform.SetLocation(Row->Points[0]);
				NodeTransform.SetScale3D(InstanceScale * 1.5f); // 노드는 정점형이므로 좀 더 굵게 표시
				ISMCompNode->AddInstance(NodeTransform);

				CreatedComponents.Add(FString::Printf(TEXT("Node_%s"), *ID), ISMCompNode);
			}
		}
	}

	// 8. DT_C1_Light (신호등) ➡️ 구체만 파랑으로 표시
	DrawLayerHelper(DT_C1_Light, nullptr, nullptr, FallbackSphereMesh, BlueMat, (FHDMapC1LightRow*)nullptr);

	UE_LOG(LogTemp, Log, TEXT("[AHDMapVisualizer] HDMap Visualization completed. Spawend %d components."), CreatedComponents.Num());
}

void AHDMapVisualizer::ClearVisualization()
{
	for (auto& Elem : CreatedComponents)
	{
		if (UInstancedStaticMeshComponent* ISMComp = Elem.Value)
		{
			if (ISMComp->IsValidLowLevel())
			{
				ISMComp->DestroyComponent();
			}
		}
	}
	CreatedComponents.Empty();

	TArray<UActorComponent*> ISMComponents;
	GetComponents(UInstancedStaticMeshComponent::StaticClass(), ISMComponents);
	for (UActorComponent* Comp : ISMComponents)
	{
		Comp->DestroyComponent();
	}

	UE_LOG(LogTemp, Log, TEXT("[AHDMapVisualizer] Cleared all visualized components."));
}

bool AHDMapVisualizer::GetPointsByLineID(const FString& LineID, TArray<FVector>& OutPoints) const
{
	TArray<UDataTable*> TargetTables;
	TargetTables.Add(DT_A2_Link);
	TargetTables.Add(DT_B2_Lane);
	TargetTables.Add(DT_C3_Protection);
	TargetTables.Add(DT_A4_Subsidiary);
	TargetTables.Add(DT_Sidewalk);
	TargetTables.Add(DT_B3_Mark);
	TargetTables.Add(DT_C4_SpeedBump);
	TargetTables.Add(DT_C5_Barrier);
	TargetTables.Add(DT_C6_Post);
	TargetTables.Add(DT_A1_Node);

	for (UDataTable* Table : TargetTables)
	{
		if (!Table) continue;

		FTableRowBase* FoundRow = Table->FindRow<FTableRowBase>(FName(*LineID), TEXT("HDMapSearch"));
		if (FoundRow)
		{
			if (Table == DT_A2_Link)
			{
				OutPoints = ((FHDMapA2LinkRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_B2_Lane)
			{
				OutPoints = ((FHDMapB2LaneRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_C3_Protection)
			{
				OutPoints = ((FHDMapC3ProtectionRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_A4_Subsidiary)
			{
				OutPoints = ((FHDMapA4SubsidiaryRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_Sidewalk)
			{
				OutPoints = ((FHDMapSidewalkRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_B3_Mark)
			{
				OutPoints = ((FHDMapB3MarkRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_C4_SpeedBump)
			{
				OutPoints = ((FHDMapC4SpeedBumpRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_C5_Barrier)
			{
				OutPoints = ((FHDMapC5BarrierRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_C6_Post)
			{
				OutPoints = ((FHDMapC6PostRow*)FoundRow)->Points;
				return true;
			}
			else if (Table == DT_A1_Node)
			{
				OutPoints = ((FHDMapA1NodeRow*)FoundRow)->Points;
				return true;
			}
		}
	}

	return false;
}

TArray<FString> AHDMapVisualizer::GetAvailableLineIDs() const
{
	TArray<FString> IDs;
	CreatedComponents.GetKeys(IDs);

	TSet<FString> UniqueIDs;
	for (const FString& FullKey : IDs)
	{
		FString CleanID = FullKey;
		if (FullKey.StartsWith(TEXT("Head_"))) CleanID = FullKey.RightChop(5);
		else if (FullKey.StartsWith(TEXT("Body_"))) CleanID = FullKey.RightChop(5);
		else if (FullKey.StartsWith(TEXT("Joint_"))) CleanID = FullKey.RightChop(6);
		else if (FullKey.StartsWith(TEXT("Node_"))) CleanID = FullKey.RightChop(5);
		
		UniqueIDs.Add(CleanID);
	}

	return UniqueIDs.Array();
}

void AHDMapVisualizer::DrawLineSegment(
	UInstancedStaticMeshComponent* ISMCompHead,
	UInstancedStaticMeshComponent* ISMCompBody,
	UInstancedStaticMeshComponent* ISMCompJoint,
	const TArray<FVector>& Points)
{
	if (Points.Num() == 0) return;

	// 1. 마디(Joint) 정점 위치에 구체 배치
	UStaticMesh* TargetSphereMesh = SphereStaticMesh;
	if (!TargetSphereMesh)
	{
		TargetSphereMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	}

	if (ISMCompJoint && TargetSphereMesh)
	{
		for (const FVector& Pt : Points)
		{
			FTransform JointTransform;
			JointTransform.SetLocation(Pt);
			JointTransform.SetScale3D(InstanceScale * 0.8f);
			ISMCompJoint->AddInstance(JointTransform);
		}
	}

	if (Points.Num() < 2) return;

	UStaticMesh* TargetLinkMesh = LinkStaticMesh;
	if (!TargetLinkMesh)
	{
		TargetLinkMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")));
	}

	UStaticMesh* TargetLaneMesh = LaneStaticMesh;
	if (!TargetLaneMesh)
	{
		TargetLaneMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	}

	// 2. 포인트 세그먼트 위에 화살표(피라미드 + 얇은 큐브 몸통) 배치
	for (int32 i = 0; i < Points.Num() - 1; ++i)
	{
		FVector StartPt = Points[i];
		FVector EndPt = Points[i + 1];

		FVector Direction = EndPt - StartPt;
		float SegmentLength = Direction.Size();

		if (SegmentLength < 0.1f) continue;

		Direction.Normalize();

		float CurrentDistance = 0.f;
		while (CurrentDistance < SegmentLength)
		{
			FVector SpawnLocation = StartPt + (Direction * CurrentDistance);
			FRotator SpawnRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();

			// 2.1) 피라미드(Arrow Head) 인스턴스
			if (ISMCompHead && TargetLinkMesh)
			{
				FTransform HeadTransform;
				HeadTransform.SetLocation(SpawnLocation);
				HeadTransform.SetRotation(SpawnRotation.Quaternion());
				HeadTransform.SetScale3D(InstanceScale);
				ISMCompHead->AddInstance(HeadTransform);
			}

			// 2.2) 얇은 큐브(Arrow Body) 인스턴스
			if (ISMCompBody && TargetLaneMesh)
			{
				FTransform BodyTransform;
				BodyTransform.SetLocation(SpawnLocation);
				BodyTransform.SetRotation(SpawnRotation.Quaternion());
				BodyTransform.SetScale3D(InstanceScale * FVector(0.3f, 0.3f, 0.8f));
				ISMCompBody->AddInstance(BodyTransform);
			}

			CurrentDistance += PointSpacing;

			if (PointSpacing < 10.0f)
			{
				break;
			}
		}
	}
}

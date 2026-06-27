// Fill out your copyright notice in the Description page of Project Settings.


#include "HDMapMeshGenerator.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "LandscapeEdit.h"       // FLandscapeEditDataInterface
#include "LandscapeInfo.h"       // ULandscapeInfo
#include "LandscapeDataAccess.h" // LANDSCAPE_ZSCALE
#endif

// 델로네 삼각분할 내부 자료구조
enum class EHDMapVertexType : uint8
{
	Curb,    // C3 외곽 연석
	Link,    // A2 링크 중심선 및 B2 차선선
	Node     // A1 교차로 노드
};

struct FDisjointSet
{
	TArray<int32> Parent;
	FDisjointSet(int32 Size)
	{
		Parent.AddUninitialized(Size);
		for (int32 i = 0; i < Size; ++i) Parent[i] = i;
	}
	int32 Find(int32 i)
	{
		if (Parent[i] == i) return i;
		Parent[i] = Find(Parent[i]);
		return Parent[i];
	}
	void Union(int32 i, int32 j)
	{
		int32 RootI = Find(i);
		int32 RootJ = Find(j);
		if (RootI != RootJ)
		{
			Parent[RootI] = RootJ;
		}
	}
};

struct FTriangle2D
{
	int32 V0, V1, V2;
	bool bDead;
	FTriangle2D(int32 InV0, int32 InV1, int32 InV2) : V0(InV0), V1(InV1), V2(InV2), bDead(false) {}
};

struct FEdge2D
{
	int32 V0, V1;
	FEdge2D(int32 InV0, int32 InV1)
	{
		// 정점 인덱스를 크기순으로 정렬하여 에지의 고유성 보장
		if (InV0 < InV1)
		{
			V0 = InV0;
			V1 = InV1;
		}
		else
		{
			V0 = InV1;
			V1 = InV0;
		}
	}

	bool operator==(const FEdge2D& Other) const
	{
		return V0 == Other.V0 && V1 == Other.V1;
	}
};


AHDMapMeshGenerator::AHDMapMeshGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	// 기본값 세팅
	DefaultRoadWidth = 600.f;   // 기본 도로 폭 6m (600cm)
	RoadHeight = 50.f;          // 도로 기본 수직 두께 50cm
	LaneWidth = 300.f;          // 기본 차선 폭 3m (300cm)
	bUseDelaunay = true;        // 기본으로 델로네 삼각분할 생성 모드 사용
	bOnlyUseMapData = true;     // 기본으로 오직 MapData 다각형만 사용
	bSnapToLandscape = true;    // 기본으로 지형 스냅 켬
	TunnelRoadZ = -1500.f;      // 기본 지하 도로 깊이 -15m
	SampleDistance = 1000.f;    // 중심선 리샘플링 기본 간격 10m (1000cm)
	GridSpacing = 300.f;        // 내부 그리드 생성 간격 기본값 3m (300cm)
	bEnableGridRefinement = true; // 기본으로 내부 그리드 정점 생성 활성화
	WeldDistance = 10.f;        // 10cm 내의 점들은 동일 정점으로 병합
	MaxEdgeLength = 1500.f;     // 삼각형 한 변의 최대 길이 15m 제한
	MinAngleDegree = 3.f;       // 내각 제한 최소 3도 (매우 길쭉하고 얇은 노이즈 삼각형 제거)
	EdgeSubdivisionCount = 5;   // 기본적으로 각 에지를 5등분으로 분할
	ZOffset = 30.f;             // 지형과의 깜빡임(Z-fighting) 방지를 위한 높이 보정치 30cm
	GridSize = 5000.f;          // 대규모 연산 부하 개선을 위한 공간 격자 크기 (50m)
	OutputSidewalkDynamicMeshActor = nullptr;
	SidewalkHeight = 20.f;
	SidewalkZOffset = 30.f;
	SidewalkGridSpacing = 200.f;
	bEnableSidewalkGridRefinement = true;
	SaveAssetPath = TEXT("/Game/HDMap/SM_NamsanRoad");
	SaveSidewalkAssetPath = TEXT("/Game/HDMap/SM_NamsanSidewalk");
	CarveZOffset = -5.f;       // 도로 노면보다 5cm 아래까지 깎기
	CarveFeatherRadius = 300.f; // 경계 바깥 3m 구간 Feather 블렌딩

	// 차선 기본값 세팅
	OutputLaneDynamicMeshActor = nullptr;
	WhiteMaterial = nullptr;
	YellowMaterial = nullptr;
	BlueMaterial = nullptr;
	LaneMarkWidth = 15.f;       // 15cm
	LaneMarkGap = 10.f;         // 10cm
	LaneMarkZOffset = 1.0f;     // 1cm (Z-fighting 방지 및 밀착 노면 위 돌출)
	LaneSampleDistance = 100.f; // 1m 간격으로 조밀하게 곡선화 리샘플링
	LaneDashedSolidLength = 300.f; // 3m
	LaneDashedSpaceLength = 500.f; // 5m (그 외 점선 기본값)
	SaveLaneAssetPath = TEXT("/Game/HDMap/SM_NamsanLane");

	// 과속 방지턱 기본값 초기화
	SpeedBumpHeight = 10.f;
	SpeedBumpGridSpacing = 30.f;
	SpeedBumpZOffset = 1.0f;
	SpeedBumpMaterial = nullptr;
	OutputSpeedBumpDynamicMeshActor = nullptr;
	SaveSpeedBumpAssetPath = TEXT("/Game/HDMap/SM_NamsanSpeedBump");

	// 도장 각인 기본값 초기화
	bEnableB3Stamping = true;
	B3StampHeight = 0.1f; // 0.1cm (1mm 미세 오프셋으로 Z-Fighting 방지 및 도로 밀착)

	// 노면표시 기본값 초기화
	MarkAtlasMaterial = nullptr;
	CrosswalkMaterial = nullptr;
	MarkZOffset = 1.5f;

	OutputMarkDynamicMeshActor = nullptr;
	SaveMarkAssetPath = TEXT("/Game/HDMap/SM_NamsanMark");
}


void AHDMapMeshGenerator::BeginPlay()
{
	Super::BeginPlay();
}

void AHDMapMeshGenerator::GenerateRoadMesh()
{
	if (!VisualizerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] VisualizerActor is not specified."));
		return;
	}

	if (!OutputDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] OutputDynamicMeshActor is not specified."));
		return;
	}

	// 0. 출력 Dynamic Mesh Actor의 트랜스폼을 Identity로 리셋
	OutputDynamicMeshActor->SetActorTransform(FTransform::Identity);

	FTransform VisTransform = VisualizerActor->GetActorTransform();

	// 전체 메쉬에 누적될 글로벌 정점 및 소속 링크 정보 구조체
	struct FGlobalVertexInfo
	{
		FVector Position;
		FString LinkID;
	};

	TArray<FGlobalVertexInfo> GlobalVerticesInfo;
	TArray<FIntVector> GlobalTriangles;
	
	// 정점을 안전하게 병합하며 추가하는 헬퍼 람다
	auto FindOrAddVertex = [&](const FVector& NewVert, float MaxDistance, const FString& CurrentLinkID) -> int32
	{
		for (int32 Index = 0; Index < GlobalVerticesInfo.Num(); ++Index)
		{
			// 델로네 모드가 아닐 때(리본 압출 모드일 때)만 동일 링크 내부의 정점 간 병합을 절대 금지 (스파게티 꼬임 방지)
			if (!bUseDelaunay && !CurrentLinkID.IsEmpty() && GlobalVerticesInfo[Index].LinkID.Equals(CurrentLinkID))
			{
				continue;
			}

			// XY 거리 기준 병합 검사 (이종 링크 간 병합)
			if (FVector::DistXY(GlobalVerticesInfo[Index].Position, NewVert) <= MaxDistance)
			{
				if (FMath::Abs(GlobalVerticesInfo[Index].Position.Z - NewVert.Z) < 200.f)
				{
					return Index;
				}
			}
		}

		FGlobalVertexInfo NewInfo;
		NewInfo.Position = NewVert;
		NewInfo.LinkID = CurrentLinkID;
		return GlobalVerticesInfo.Add(NewInfo);
	};

	// 링크 ID별 좌측/우측 글로벌 정점 인덱스 배열을 저장하는 로컬 맵
	TMap<FString, TArray<int32>> LinkLeftVertexIndices;
	TMap<FString, TArray<int32>> LinkRightVertexIndices;

	// A2_LINK 도로 중심선 데이터 테이블 획득
	UDataTable* LinkTable = VisualizerActor->DT_A2_Link;
	if (!LinkTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] DT_A2_Link is not mapped."));
		return;
	}

	TArray<FHDMapA2LinkRow*> TargetRows;
	if (SelectedLineIDs.Num() > 0)
	{
		for (const FString& SelID : SelectedLineIDs)
		{
			FHDMapA2LinkRow* FoundRow = LinkTable->FindRow<FHDMapA2LinkRow>(FName(*SelID), TEXT("HDMapMeshGen_LinkSearch"));
			if (FoundRow)
			{
				TargetRows.Add(FoundRow);
			}
		}
	}
	else
	{
		LinkTable->GetAllRows<FHDMapA2LinkRow>(TEXT("HDMapMeshGen_AllA2Search"), TargetRows);
	}

	// 1. 빠른 데이터 탐색을 위해 모든 도로 링크 정보를 ID별로 로컬 맵에 캐싱
	TMap<FString, FHDMapA2LinkRow*> LinkRowMap;
	for (FHDMapA2LinkRow* LinkRow : TargetRows)
	{
		if (LinkRow)
		{
			LinkRowMap.Add(LinkRow->ID, LinkRow);
		}
	}

	float HalfWidth = DefaultRoadWidth / 2.0f;

	if (bUseDelaunay)
	{
		if (bOnlyUseMapData)
		{
			UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Generating road mesh using ONLY MapData (HDMapDataTable)."));
			UDataTable* DrivewayTable = VisualizerActor->HDMapDataTable;
			if (!DrivewayTable)
			{
				UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] VisualizerActor->HDMapDataTable is NULL!"));
				return;
			}

			TArray<FHDMapLineRow*> AllDriveways;
			DrivewayTable->GetAllRows<FHDMapLineRow>(TEXT("HDMapMeshGen_OnlyMapData"), AllDriveways);
			
			UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Loaded %d driveways from HDMapDataTable."), AllDriveways.Num());

			// ═══════════════════════════════════════════════════════════════════
			// 패스 1: 모든 다각형 경계점 수집 (월드 좌표 변환)
			// ═══════════════════════════════════════════════════════════════════
			TArray<TArray<FVector>> PolyBoundaries;     // 각 다각형의 월드 좌표 경계점
			TArray<bool>            PolyIsTunnel;        // 터널 여부
			TArray<FHDMapLineRow*>  PolyRows;            // 원본 데이터 행 참조

			for (FHDMapLineRow* DwRow : AllDriveways)
			{
				if (!DwRow || DwRow->ID.Equals(TEXT("ORIGIN")) || DwRow->Points.Num() < 3) continue;

				// 지하 도로(터널) 여부 감색
				bool bIsTunnel = TunnelRoadIDs.Contains(DwRow->ID);

				TArray<FVector> LocalVertices;
				LocalVertices.Reserve(DwRow->Points.Num() * 2);
				for (const FVector& Pt : DwRow->Points)
				{
					FVector PtWorld = VisTransform.TransformPosition(Pt);
					
					if (bIsTunnel)
					{
						// 지하 도로는 TunnelRoadZ에 맞춤
						PtWorld.Z = TunnelRoadZ + ZOffset;
					}
					else if (bSnapToLandscape)
					{
						// 지상 도로는 월드 지형 높이에 밀착
						float LandZ = GetLandscapeZ(PtWorld);
						PtWorld.Z = LandZ + ZOffset;
					}
					else
					{
						PtWorld.Z += ZOffset;
					}

					LocalVertices.Add(PtWorld);
				}

				PolyBoundaries.Add(MoveTemp(LocalVertices));
				PolyIsTunnel.Add(bIsTunnel);
				PolyRows.Add(DwRow);
			}

			UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Collected %d polygons. Starting global boundary vertex weld..."), PolyBoundaries.Num());

			// ═══════════════════════════════════════════════════════════════════
			// 패스 1.5: 글로벌 경계 정점 병합 (Disjoint Set Weld, 원본 정점 대상)
			// ═══════════════════════════════════════════════════════════════════
			// 모든 다각형의 각 정점 중 서로 다른 다각형 소속이면서 XY 거리가 WeldDistance 이내인
			// 짝이 맞는 정점들을 하나의 대표 좌표(평균값)로 완벽히 병합합니다.
			struct FGlobalVertexRef
			{
				int32 PolyIdx;
				int32 VertIdx;
				FVector* ValuePtr;
			};

			TArray<FGlobalVertexRef> GlobalVerts;
			for (int32 PolyIdx = 0; PolyIdx < PolyBoundaries.Num(); ++PolyIdx)
			{
				for (int32 VertIdx = 0; VertIdx < PolyBoundaries[PolyIdx].Num(); ++VertIdx)
				{
					GlobalVerts.Add({ PolyIdx, VertIdx, &PolyBoundaries[PolyIdx][VertIdx] });
				}
			}

			const int32 TotalVerts = GlobalVerts.Num();
			if (TotalVerts > 0)
			{
				FDisjointSet DS(TotalVerts);
				for (int32 i = 0; i < TotalVerts; ++i)
				{
					for (int32 j = i + 1; j < TotalVerts; ++j)
					{
						// 같은 다각형의 자체 인접 에지 정점을 weld하는 것을 방지하기 위해 다각형 인덱스가 다른 경우에만 weld 수행
						if (GlobalVerts[i].PolyIdx == GlobalVerts[j].PolyIdx) continue;

						if (FVector::DistXY(*GlobalVerts[i].ValuePtr, *GlobalVerts[j].ValuePtr) < WeldDistance)
						{
							DS.Union(i, j);
						}
					}
				}

				TArray<FVector> GroupSum;
				TArray<int32> GroupCount;
				GroupSum.AddZeroed(TotalVerts);
				GroupCount.AddZeroed(TotalVerts);

				for (int32 i = 0; i < TotalVerts; ++i)
				{
					int32 Root = DS.Find(i);
					GroupSum[Root] += *GlobalVerts[i].ValuePtr;
					GroupCount[Root]++;
				}

				for (int32 i = 0; i < TotalVerts; ++i)
				{
					int32 Root = DS.Find(i);
					FVector AvgPt = GroupSum[Root] / (float)GroupCount[Root];
					*GlobalVerts[i].ValuePtr = AvgPt;
				}

				UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Global boundary vertex weld done. Processed %d vertices."), TotalVerts);
			}

			// ═══════════════════════════════════════════════════════════════════
			// 패스 2: 경계 정점 중복 제거 (모서리 밀집 해소)
			// ═══════════════════════════════════════════════════════════════════
			// 병합된 정점 중 연속된 두 정점이 WeldDistance 이내에 있으면 하나를 제거
			for (TArray<FVector>& Boundary : PolyBoundaries)
			{
				for (int32 i = Boundary.Num() - 1; i > 0; --i)
				{
					if (FVector::DistXY(Boundary[i], Boundary[i - 1]) < WeldDistance)
					{
						Boundary.RemoveAt(i);
					}
				}
				// 첫 정점과 마지막 정점도 검사 (닫힌 다각형)
				if (Boundary.Num() > 1 && FVector::DistXY(Boundary[0], Boundary.Last()) < WeldDistance)
				{
					Boundary.RemoveAt(Boundary.Num() - 1);
				}
			}

			// ═══════════════════════════════════════════════════════════════════
			// 패스 2.5: 에지 분할 (Edge Subdivision - Weld 및 중복 제거 완료 후 사후 분할)
			// ═══════════════════════════════════════════════════════════════════
			// 각 다각형의 모서리를 EdgeSubdivisionCount 개수로 나눕니다.
			if (EdgeSubdivisionCount > 1)
			{
				TArray<TArray<FVector>> SubdividedBoundaries;
				SubdividedBoundaries.Reserve(PolyBoundaries.Num());
				for (const TArray<FVector>& Boundary : PolyBoundaries)
				{
					TArray<FVector> NewBoundary;
					const int32 NumPts = Boundary.Num();
					if (NumPts < 3)
					{
						SubdividedBoundaries.Add(Boundary);
						continue;
					}
					NewBoundary.Reserve(NumPts * EdgeSubdivisionCount);
					for (int32 i = 0; i < NumPts; ++i)
					{
						const FVector& V0 = Boundary[i];
						const FVector& V1 = Boundary[(i + 1) % NumPts];
						NewBoundary.Add(V0);
						for (int32 Step = 1; Step < EdgeSubdivisionCount; ++Step)
						{
							float T = (float)Step / (float)EdgeSubdivisionCount;
							FVector SubdivPt = FMath::Lerp(V0, V1, T);
							NewBoundary.Add(SubdivPt);
						}
					}
					SubdividedBoundaries.Add(MoveTemp(NewBoundary));
				}
				PolyBoundaries = MoveTemp(SubdividedBoundaries);
			}

			// 에지별 출현 횟수 카운팅을 위한 맵과 캐싱 인덱스 배열 정의
			auto GetEdgeKey = [](int32 V0, int32 V1) -> uint64
			{
				int32 MinV = FMath::Min(V0, V1);
				int32 MaxV = FMath::Max(V0, V1);
				return ((uint64)MinV << 32) | (uint32)MaxV;
			};

			TArray<TArray<int32>> PolyBottomIndices;
			TArray<TArray<int32>> PolyTopIndices;
			PolyBottomIndices.AddDefaulted(PolyBoundaries.Num());
			PolyTopIndices.AddDefaulted(PolyBoundaries.Num());

			// 정점 인덱스를 전 다각형에 대해 일괄적으로 미리 확보
			for (int32 PolyIdx = 0; PolyIdx < PolyBoundaries.Num(); ++PolyIdx)
			{
				TArray<FVector>& BoundaryPoints = PolyBoundaries[PolyIdx];
				int32 BoundaryCount = BoundaryPoints.Num();

				PolyBottomIndices[PolyIdx].AddUninitialized(BoundaryCount);
				PolyTopIndices[PolyIdx].AddUninitialized(BoundaryCount);

				FHDMapLineRow* DwRow = PolyRows[PolyIdx];

				for (int32 v = 0; v < BoundaryCount; ++v)
				{
					FVector BasePos = BoundaryPoints[v];
					
					FVector BottomPos = BasePos; // 도로 바닥 높이
					PolyBottomIndices[PolyIdx][v] = FindOrAddVertex(BottomPos, WeldDistance, DwRow->ID);

					FVector TopPos = BasePos;
					TopPos.Z += RoadHeight; // 도로 상판 높이 (두께만큼 위로 돌출)
					PolyTopIndices[PolyIdx][v] = FindOrAddVertex(TopPos, WeldDistance, DwRow->ID);
				}
			}

			// 에지별 출현 횟수 카운팅
			TMap<uint64, int32> EdgeUsageMap;
			for (int32 PolyIdx = 0; PolyIdx < PolyBoundaries.Num(); ++PolyIdx)
			{
				int32 BoundaryCount = PolyBoundaries[PolyIdx].Num();
				if (BoundaryCount < 3) continue;

				const TArray<int32>& BottomIndices = PolyBottomIndices[PolyIdx];

				for (int32 i = 0; i < BoundaryCount; ++i)
				{
					int32 next_i = (i + 1) % BoundaryCount;
					int32 B0 = BottomIndices[i];
					int32 B1 = BottomIndices[next_i];

					uint64 EdgeKey = GetEdgeKey(B0, B1);
					EdgeUsageMap.FindOrAdd(EdgeKey, 0)++;
				}
			}

			// ═══════════════════════════════════════════════════════════════════
			// 패스 3: 동기화된 정점으로 삼각분할 (볼륨화 구현)
			// ═══════════════════════════════════════════════════════════════════
			for (int32 PolyIdx = 0; PolyIdx < PolyBoundaries.Num(); ++PolyIdx)
			{
				TArray<FVector>& LocalVertices = PolyBoundaries[PolyIdx];
				const bool bIsTunnel = PolyIsTunnel[PolyIdx];
				FHDMapLineRow* DwRow = PolyRows[PolyIdx];

				TArray<FVector> SyncBoundary = LocalVertices;
				int32 BoundaryCount = SyncBoundary.Num();

				const TArray<int32>& BottomGlobalIndices = PolyBottomIndices[PolyIdx];
				const TArray<int32>& TopGlobalIndices = PolyTopIndices[PolyIdx];

				// 2) 도로 내부 그리드 정점 자동 추가 (Grid Refinement)
				if (!bIsTunnel && bSnapToLandscape && bEnableGridRefinement && DwRow->Points.Num() >= 3)
				{
					FBox2D PolyBox(EForceInit::ForceInit);
					for (const FVector& Pt : DwRow->Points)
					{
						PolyBox += FVector2D(Pt.X, Pt.Y);
					}

					float LocalGridSpacing = FMath::Max(50.f, GridSpacing); 

					for (float GridX = PolyBox.Min.X + LocalGridSpacing; GridX < PolyBox.Max.X; GridX += LocalGridSpacing)
					{
						for (float GridY = PolyBox.Min.Y + LocalGridSpacing; GridY < PolyBox.Max.Y; GridY += LocalGridSpacing)
						{
							FVector2D TestPt(GridX, GridY);
							if (IsPointInPolygon2D(TestPt, DwRow->Points))
							{
								FVector PtLocal(GridX, GridY, 0.f);
								FVector PtWorld = VisTransform.TransformPosition(PtLocal);
								
								float LandZ = GetLandscapeZ(PtWorld);
								PtWorld.Z = LandZ + ZOffset;

								LocalVertices.Add(PtWorld);
							}
						}
					}
				}

				if (LocalVertices.Num() < 3) continue;

				TArray<FIntVector> RawTriangles;
				if (RunDelaunayTriangulation(LocalVertices, RawTriangles))
				{
					TArray<int32> TopDelaunayGlobalIndices;
					TArray<int32> BottomDelaunayGlobalIndices;
					TopDelaunayGlobalIndices.AddUninitialized(LocalVertices.Num());
					BottomDelaunayGlobalIndices.AddUninitialized(LocalVertices.Num());

					// 경계 정점들은 이미 캐싱된 상판/하판 인덱스를 매칭
					for (int32 v = 0; v < BoundaryCount; ++v)
					{
						TopDelaunayGlobalIndices[v] = TopGlobalIndices[v];
						BottomDelaunayGlobalIndices[v] = BottomGlobalIndices[v];
					}
					// 내부 그리드 정점들은 새로이 FindOrAddVertex 수행
					for (int32 v = BoundaryCount; v < LocalVertices.Num(); ++v)
					{
						FVector BottomPos = LocalVertices[v];
						BottomDelaunayGlobalIndices[v] = FindOrAddVertex(BottomPos, WeldDistance, DwRow->ID);

						FVector TopPos = LocalVertices[v];
						TopPos.Z += RoadHeight;
						TopDelaunayGlobalIndices[v] = FindOrAddVertex(TopPos, WeldDistance, DwRow->ID);
					}

					for (const FIntVector& Tri : RawTriangles)
					{
						FVector V0 = LocalVertices[Tri.X];
						FVector V1 = LocalVertices[Tri.Y];
						FVector V2 = LocalVertices[Tri.Z];

						FVector Centroid = (V0 + V1 + V2) / 3.0f;
						FVector2D Centroid2D(Centroid.X, Centroid.Y);

						if (IsPointInPolygon2D(Centroid2D, SyncBoundary))
						{
							FVector E0 = V1 - V0;
							FVector E1 = V2 - V0;
							FVector CrossVal = FVector::CrossProduct(E0, E1);

							int32 FinalY = Tri.Y;
							int32 FinalZ = Tri.Z;
							if (CrossVal.Z > 0.f)
							{
								FinalY = Tri.Z;
								FinalZ = Tri.Y;
							}

							// 1) 상판(Top Face) 삼각형 추가
							int32 G_TopX = TopDelaunayGlobalIndices[Tri.X];
							int32 G_TopY = TopDelaunayGlobalIndices[FinalY];
							int32 G_TopZ = TopDelaunayGlobalIndices[FinalZ];
							if (G_TopX != G_TopY && G_TopY != G_TopZ && G_TopZ != G_TopX)
							{
								GlobalTriangles.Add(FIntVector(G_TopX, G_TopY, G_TopZ));
							}

							// 2) 하판(Bottom Face) 삼각형 추가 (아래를 바라보도록 Winding Order 반전)
							int32 G_BotX = BottomDelaunayGlobalIndices[Tri.X];
							int32 G_BotY = BottomDelaunayGlobalIndices[FinalY];
							int32 G_BotZ = BottomDelaunayGlobalIndices[FinalZ];
							if (G_BotX != G_BotY && G_BotY != G_BotZ && G_BotZ != G_BotX)
							{
								GlobalTriangles.Add(FIntVector(G_BotX, G_BotZ, G_BotY));
							}
						}
					}
				}

				// 3) 수직 측벽(Side Wall) 생성 (공유하지 않는 외곽 에지만 생성)
				if (BoundaryCount >= 3)
				{
					for (int32 i = 0; i < BoundaryCount; ++i)
					{
						int32 next_i = (i + 1) % BoundaryCount;

						int32 B0 = BottomGlobalIndices[i];
						int32 B1 = BottomGlobalIndices[next_i];
						int32 T0 = TopGlobalIndices[i];
						int32 T1 = TopGlobalIndices[next_i];

						uint64 EdgeKey = GetEdgeKey(B0, B1);
						if (EdgeUsageMap.FindRef(EdgeKey) > 1)
						{
							continue;
						}

						if (B0 != B1 && B1 != T1 && T1 != B0)
						{
							GlobalTriangles.Add(FIntVector(B0, B1, T1));
						}
						if (B0 != T1 && T1 != T0 && T0 != B0)
						{
							GlobalTriangles.Add(FIntVector(B0, T1, T0));
						}
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Generating Delaunay road mesh for %d links. SampleDistance: %.1f"), TargetRows.Num(), SampleDistance);

			// 선형 데이터(Points)를 특정 간격(SampleDist)으로 리샘플링하여 점들을 추출하는 헬퍼 함수
		auto ResamplePoints = [](const TArray<FVector>& OriginalPoints, float SampleDist, TArray<FVector>& OutResampled)
		{
			if (OriginalPoints.Num() < 2) return;

			OutResampled.Add(OriginalPoints[0]);
			float AccumulatedDistance = 0.f;
			float TargetDistance = SampleDist;

			for (int32 idx = 0; idx < OriginalPoints.Num() - 1; ++idx)
			{
				FVector P0 = OriginalPoints[idx];
				FVector P1 = OriginalPoints[idx + 1];
				float SegmentLen = FVector::Dist(P0, P1);

				while (AccumulatedDistance + SegmentLen >= TargetDistance)
				{
					float Ratio = (TargetDistance - AccumulatedDistance) / SegmentLen;
					FVector InterpolatedPt = FMath::Lerp(P0, P1, Ratio);
					OutResampled.Add(InterpolatedPt);
					TargetDistance += SampleDist;
				}

				AccumulatedDistance += SegmentLen;
			}

			if (FVector::Dist(OutResampled.Last(), OriginalPoints.Last()) > SampleDist * 0.1f)
			{
				OutResampled.Add(OriginalPoints.Last());
			}
		};

		float SearchRadius = DefaultRoadWidth * 2.0f;
		float MaxZDiff = 500.f; // 5m 수직 고도 제한

		// 1. A2_LINK 주변 리소스 (C3 연석, A1 교차로, A3 도로 면 다각형) 데이터 사전 수집
		// 1) A1 노드 수집
		TArray<FHDMapA1NodeRow*> AssociatedNodes;
		UDataTable* NodeTable = VisualizerActor->DT_A1_Node;
		if (NodeTable)
		{
			TArray<FHDMapA1NodeRow*> AllNodes;
			NodeTable->GetAllRows<FHDMapA1NodeRow>(TEXT("HDMapMeshGen_AllNodes"), AllNodes);
			for (FHDMapA1NodeRow* NodeRow : AllNodes)
			{
				if (!NodeRow || NodeRow->ID.Equals(TEXT("ORIGIN")) || NodeRow->Points.Num() == 0) continue;
				
				bool bIsClose = false;
				FVector NodePt = NodeRow->Points[0];
				for (FHDMapA2LinkRow* LinkRow : TargetRows)
				{
					if (LinkRow->Points.Num() < 2) continue;
					FVector StartPt = LinkRow->Points[0];
					FVector EndPt = LinkRow->Points.Last();
					
					if ((FVector::Dist2D(NodePt, StartPt) <= SearchRadius && FMath::Abs(NodePt.Z - StartPt.Z) <= MaxZDiff) ||
						(FVector::Dist2D(NodePt, EndPt) <= SearchRadius && FMath::Abs(NodePt.Z - EndPt.Z) <= MaxZDiff))
					{
						bIsClose = true;
						break;
					}
				}
				if (bIsClose)
				{
					AssociatedNodes.Add(NodeRow);
				}
			}
		}

		// 2) C3 연석 수집
		TArray<FHDMapC3ProtectionRow*> AssociatedCurbs;
		UDataTable* CurbTable = VisualizerActor->DT_C3_Protection;
		if (CurbTable)
		{
			TArray<FHDMapC3ProtectionRow*> AllCurbs;
			CurbTable->GetAllRows<FHDMapC3ProtectionRow>(TEXT("HDMapMeshGen_AllCurbs"), AllCurbs);
			for (FHDMapC3ProtectionRow* CurbRow : AllCurbs)
			{
				if (!CurbRow || CurbRow->ID.Equals(TEXT("ORIGIN")) || CurbRow->Points.Num() == 0) continue;
				if (CurbRow->IsCentral == 1) continue; // 중앙분리대 필터링
				
				bool bIsClose = false;
				for (const FVector& CurbPt : CurbRow->Points)
				{
					for (FHDMapA2LinkRow* LinkRow : TargetRows)
					{
						for (const FVector& LinkPt : LinkRow->Points)
						{
							if (FVector::Dist2D(CurbPt, LinkPt) <= SearchRadius &&
								FMath::Abs(CurbPt.Z - LinkPt.Z) <= MaxZDiff)
							{
								bIsClose = true;
								break;
							}
						}
						if (bIsClose) break;
					}
					if (bIsClose) break;
				}
				if (bIsClose)
				{
					AssociatedCurbs.Add(CurbRow);
				}
			}
		}

		// 3) 도로 면 다각형 수집 (도로 병합.shp 벡터 데이터 연동 - HDMapDataTable 레거시/커스텀 사용)
		TArray<FHDMapLineRow*> AssociatedDriveways;
		UDataTable* DrivewayTable = VisualizerActor->HDMapDataTable;
		if (DrivewayTable)
		{
			TArray<FHDMapLineRow*> AllDriveways;
			DrivewayTable->GetAllRows<FHDMapLineRow>(TEXT("HDMapMeshGen_AllDriveways"), AllDriveways);
			for (FHDMapLineRow* DwRow : AllDriveways)
			{
				if (!DwRow || DwRow->ID.Equals(TEXT("ORIGIN")) || DwRow->Points.Num() < 3) continue;
				
				bool bIsClose = false;
				FBox DwBox(DwRow->Points);
				for (FHDMapA2LinkRow* LinkRow : TargetRows)
				{
					FBox LinkBox(LinkRow->Points);
					if (DwBox.ExpandBy(FVector(SearchRadius, SearchRadius, MaxZDiff)).Intersect(LinkBox))
					{
						bIsClose = true;
						break;
					}
				}
				if (bIsClose)
				{
					AssociatedDriveways.Add(DwRow);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] VisualizerActor->HDMapDataTable is NULL!"));
		}

		// 2. Union-Find 기반 A2_LINK 그룹핑 (Connected Components)
		int32 NumLinks = TargetRows.Num();
		FDisjointSet DS(NumLinks);

		TArray<FBox> LinkBoxes;
		LinkBoxes.Reserve(NumLinks);
		for (int32 i = 0; i < NumLinks; ++i)
		{
			LinkBoxes.Add(FBox(TargetRows[i]->Points));
		}

		for (int32 i = 0; i < NumLinks; ++i)
		{
			for (int32 j = i + 1; j < NumLinks; ++j)
			{
				FHDMapA2LinkRow* LinkA = TargetRows[i];
				FHDMapA2LinkRow* LinkB = TargetRows[j];

				bool bConnected = false;
				if ((!LinkA->L_LinkID.IsEmpty() && LinkA->L_LinkID.Equals(LinkB->ID)) ||
					(!LinkA->R_LinkID.IsEmpty() && LinkA->R_LinkID.Equals(LinkB->ID)) ||
					(!LinkB->L_LinkID.IsEmpty() && LinkB->L_LinkID.Equals(LinkA->ID)) ||
					(!LinkB->R_LinkID.IsEmpty() && LinkB->R_LinkID.Equals(LinkA->ID)))
				{
					bConnected = true;
				}

				if (!bConnected)
				{
					FBox ExpandedBoxA = LinkBoxes[i].ExpandBy(FVector(SearchRadius, SearchRadius, MaxZDiff));
					if (ExpandedBoxA.Intersect(LinkBoxes[j]))
					{
						for (const FVector& PtA : LinkA->Points)
						{
							for (const FVector& PtB : LinkB->Points)
							{
								if (FVector::Dist2D(PtA, PtB) <= SearchRadius &&
									FMath::Abs(PtA.Z - PtB.Z) <= MaxZDiff)
								{
									bConnected = true;
									break;
								}
							}
							if (bConnected) break;
						}
					}
				}

				if (bConnected)
				{
					DS.Union(i, j);
				}
			}
		}

		TMap<int32, TArray<FHDMapA2LinkRow*>> A2Groups;
		for (int32 i = 0; i < NumLinks; ++i)
		{
			int32 Root = DS.Find(i);
			A2Groups.FindOrAdd(Root).Add(TargetRows[i]);
		}

		// 3. 각 그룹별 독립 델로네 삼각분할 수행
		for (auto& Pair : A2Groups)
		{
			const TArray<FHDMapA2LinkRow*>& GroupLinks = Pair.Value;
			UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Processing Group: %d links"), GroupLinks.Num());

			struct FLocalVertexInfo
			{
				FVector Position;
				EHDMapVertexType Type;
				FString SourceID;
			};
			TArray<FLocalVertexInfo> LocalVerticesInfo;

			auto FindOrAddLocalVertex = [&](const FVector& NewVert, float MaxDistance, EHDMapVertexType Type, const FString& SourceID) -> int32
			{
				for (int32 Index = 0; Index < LocalVerticesInfo.Num(); ++Index)
				{
					if (FVector::DistXY(LocalVerticesInfo[Index].Position, NewVert) <= MaxDistance)
					{
						if (FMath::Abs(LocalVerticesInfo[Index].Position.Z - NewVert.Z) < 200.f)
						{
							if (LocalVerticesInfo[Index].Type != EHDMapVertexType::Curb && Type == EHDMapVertexType::Curb)
							{
								LocalVerticesInfo[Index].Type = EHDMapVertexType::Curb;
							}
							return Index;
						}
					}
				}

				FLocalVertexInfo NewInfo;
				NewInfo.Position = NewVert;
				NewInfo.Type = Type;
				NewInfo.SourceID = SourceID;
				return LocalVerticesInfo.Add(NewInfo);
			};

			// 주변 리소스 분배 (도로 면 다각형)
			TArray<FHDMapLineRow*> LocalDriveways;
			for (FHDMapLineRow* DwRow : AssociatedDriveways)
			{
				bool bIsClose = false;
				FBox DwBox(DwRow->Points);
				for (FHDMapA2LinkRow* LinkRow : GroupLinks)
				{
					FBox LinkBox(LinkRow->Points);
					if (DwBox.ExpandBy(FVector(SearchRadius, SearchRadius, MaxZDiff)).Intersect(LinkBox))
					{
						bIsClose = true;
						break;
					}
				}
				if (bIsClose)
				{
					LocalDriveways.Add(DwRow);
				}
			}

			// 주변 리소스 분배 (A1 교차로)
			TArray<FHDMapA1NodeRow*> LocalNodes;
			for (FHDMapA1NodeRow* NodeRow : AssociatedNodes)
			{
				bool bIsClose = false;
				FVector NodePt = NodeRow->Points[0];
				for (FHDMapA2LinkRow* LinkRow : GroupLinks)
				{
					if (LinkRow->Points.Num() < 2) continue;
					FVector StartPt = LinkRow->Points[0];
					FVector EndPt = LinkRow->Points.Last();
					
					if ((FVector::Dist2D(NodePt, StartPt) <= SearchRadius && FMath::Abs(NodePt.Z - StartPt.Z) <= MaxZDiff) ||
						(FVector::Dist2D(NodePt, EndPt) <= SearchRadius && FMath::Abs(NodePt.Z - EndPt.Z) <= MaxZDiff))
					{
						bIsClose = true;
						break;
					}
				}
				if (bIsClose)
				{
					LocalNodes.Add(NodeRow);
				}
			}

			// 주변 리소스 분배 (C3 연석 - 면 데이터가 없을 때만 외곽선 후보로 활용)
			TArray<FHDMapC3ProtectionRow*> LocalCurbs;
			if (LocalDriveways.Num() == 0)
			{
				for (FHDMapC3ProtectionRow* CurbRow : AssociatedCurbs)
				{
					bool bIsClose = false;
					for (const FVector& CurbPt : CurbRow->Points)
					{
						for (FHDMapA2LinkRow* LinkRow : GroupLinks)
						{
							for (const FVector& LinkPt : LinkRow->Points)
							{
								if (FVector::Dist2D(CurbPt, LinkPt) <= SearchRadius &&
									FMath::Abs(CurbPt.Z - LinkPt.Z) <= MaxZDiff)
								{
									bIsClose = true;
									break;
								}
							}
							if (bIsClose) break;
						}
						if (bIsClose) break;
					}
					if (bIsClose)
					{
						LocalCurbs.Add(CurbRow);
					}
				}
			}

			// 정점 채우기 - 1순위: 도로 면 다각형 경계선
			UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Group Resources: Driveways=%d, Nodes=%d, Curbs=%d"), LocalDriveways.Num(), LocalNodes.Num(), LocalCurbs.Num());
			if (LocalDriveways.Num() > 0)
			{
				for (FHDMapLineRow* DwRow : LocalDriveways)
				{
					TArray<FVector> ResampledDw;
					ResamplePoints(DwRow->Points, SampleDistance, ResampledDw);
					for (const FVector& Pt : ResampledDw)
					{
						FVector PtWorld = VisTransform.TransformPosition(Pt);
						PtWorld.Z += ZOffset;
						FindOrAddLocalVertex(PtWorld, WeldDistance, EHDMapVertexType::Curb, DwRow->ID);
					}
				}
			}
			else
			{
				// 면 데이터가 없는 경우 2순위: C3 연석 경계선 수집
				for (FHDMapC3ProtectionRow* CurbRow : LocalCurbs)
				{
					TArray<FVector> ResampledCurb;
					ResamplePoints(CurbRow->Points, SampleDistance, ResampledCurb);
					for (const FVector& Pt : ResampledCurb)
					{
						FVector PtWorld = VisTransform.TransformPosition(Pt);
						PtWorld.Z += ZOffset;
						FindOrAddLocalVertex(PtWorld, WeldDistance, EHDMapVertexType::Curb, CurbRow->ID);
					}
				}
			}

			// 정점 채우기 - A1 교차로
			for (FHDMapA1NodeRow* NodeRow : LocalNodes)
			{
				FVector CenterPt = NodeRow->Points[0];
				FVector CenterPtWorld = VisTransform.TransformPosition(CenterPt);
				CenterPtWorld.Z += ZOffset;
				FindOrAddLocalVertex(CenterPtWorld, WeldDistance, EHDMapVertexType::Node, NodeRow->ID);

				FVector Offsets[4] = {
					FVector(HalfWidth, 0.f, 0.f),
					FVector(-HalfWidth, 0.f, 0.f),
					FVector(0.f, HalfWidth, 0.f),
					FVector(0.f, -HalfWidth, 0.f)
				};
				for (int32 o = 0; o < 4; ++o)
				{
					FVector OffsetPtWorld = VisTransform.TransformPosition(CenterPt + Offsets[o]);
					OffsetPtWorld.Z += ZOffset;
					FindOrAddLocalVertex(OffsetPtWorld, WeldDistance, EHDMapVertexType::Node, NodeRow->ID);
				}
			}

			// 정점 채우기 - A2 링크 및 하이브리드 가상 경점 보완 (면 데이터가 없을 때만 보완)
			for (FHDMapA2LinkRow* LinkRow : GroupLinks)
			{
				TArray<FVector> ResampledPoints;
				ResamplePoints(LinkRow->Points, SampleDistance, ResampledPoints);

				int32 NumPts = ResampledPoints.Num();
				if (NumPts < 2) continue;

				FString CurrentLinkID = LinkRow->ID;

				for (int32 i = 0; i < NumPts; ++i)
				{
					FVector Direction;
					if (i == 0)
					{
						Direction = ResampledPoints[1] - ResampledPoints[0];
					}
					else if (i == NumPts - 1)
					{
						Direction = ResampledPoints[NumPts - 1] - ResampledPoints[NumPts - 2];
					}
					else
					{
						Direction = ResampledPoints[i + 1] - ResampledPoints[i - 1];
					}

					Direction.Z = 0.0f;
					Direction.Normalize();

					FVector RightNormal = FVector(-Direction.Y, Direction.X, 0.0f);
					FVector CurrentPt = ResampledPoints[i];

					FVector CenterPtWorld = VisTransform.TransformPosition(CurrentPt);
					CenterPtWorld.Z += ZOffset;
					FindOrAddLocalVertex(CenterPtWorld, WeldDistance, EHDMapVertexType::Link, CurrentLinkID);

					// 면 데이터가 아예 없을 때만 C3 및 가상 경계 하이브리드 보완 수행
					if (LocalDriveways.Num() == 0)
					{
						bool bHasLeftCurb = false;
						bool bHasRightCurb = false;

						FVector LeftTargetPt = CurrentPt - RightNormal * HalfWidth;
						FVector RightTargetPt = CurrentPt + RightNormal * HalfWidth;

						float CurbCheckDistSqr = FMath::Square(HalfWidth * 1.5f);

						for (FHDMapC3ProtectionRow* CurbRow : LocalCurbs)
						{
							for (const FVector& CurbPt : CurbRow->Points)
							{
								if (!bHasLeftCurb && FVector::DistSquared2D(CurbPt, LeftTargetPt) <= CurbCheckDistSqr)
								{
									bHasLeftCurb = true;
								}
								if (!bHasRightCurb && FVector::DistSquared2D(CurbPt, RightTargetPt) <= CurbCheckDistSqr)
								{
									bHasRightCurb = true;
								}
								if (bHasLeftCurb && bHasRightCurb) break;
							}
							if (bHasLeftCurb && bHasRightCurb) break;
						}

						if (!bHasLeftCurb)
						{
							FVector LeftPtWorld = VisTransform.TransformPosition(LeftTargetPt);
							LeftPtWorld.Z += ZOffset;
							FindOrAddLocalVertex(LeftPtWorld, WeldDistance, EHDMapVertexType::Curb, CurrentLinkID);
						}

						if (!bHasRightCurb)
						{
							FVector RightPtWorld = VisTransform.TransformPosition(RightTargetPt);
							RightPtWorld.Z += ZOffset;
							FindOrAddLocalVertex(RightPtWorld, WeldDistance, EHDMapVertexType::Curb, CurrentLinkID);
						}
					}
				}
			}

			UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Collected Local Vertices: %d"), LocalVerticesInfo.Num());
			if (LocalVerticesInfo.Num() < 3)
			{
				UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Skipping group: Not enough vertices (needed >= 3)"));
				continue;
			}

			TArray<FVector> Vertices;
			Vertices.Reserve(LocalVerticesInfo.Num());
			for (const FLocalVertexInfo& LInfo : LocalVerticesInfo)
			{
				Vertices.Add(LInfo.Position);
			}

			TArray<FIntVector> RawTriangles;
			if (RunDelaunayTriangulation(Vertices, RawTriangles))
			{
				UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Delaunay Succeeded. Raw Triangles: %d"), RawTriangles.Num());
				TArray<int32> LocalToGlobalIndexMap;
				LocalToGlobalIndexMap.AddUninitialized(LocalVerticesInfo.Num());

				for (int32 LocalIdx = 0; LocalIdx < LocalVerticesInfo.Num(); ++LocalIdx)
				{
					const FLocalVertexInfo& LInfo = LocalVerticesInfo[LocalIdx];
					LocalToGlobalIndexMap[LocalIdx] = FindOrAddVertex(LInfo.Position, WeldDistance, LInfo.SourceID);
				}

				for (const FIntVector& Tri : RawTriangles)
				{
					FVector V0 = Vertices[Tri.X];
					FVector V1 = Vertices[Tri.Y];
					FVector V2 = Vertices[Tri.Z];

					const FLocalVertexInfo& LInfo0 = LocalVerticesInfo[Tri.X];
					const FLocalVertexInfo& LInfo1 = LocalVerticesInfo[Tri.Y];
					const FLocalVertexInfo& LInfo2 = LocalVerticesInfo[Tri.Z];

					// C3 외곽 필터 (면 데이터가 없을 때만 동작)
					if (LocalDriveways.Num() == 0)
					{
						if (LInfo0.Type == EHDMapVertexType::Curb &&
							LInfo1.Type == EHDMapVertexType::Curb &&
							LInfo2.Type == EHDMapVertexType::Curb)
						{
							continue;
						}
					}

					// 1. 다각형 내부 판정 필터 (도로 면 데이터가 있는 경우 무게중심으로 필터링)
					if (LocalDriveways.Num() > 0)
					{
						FVector Centroid = (V0 + V1 + V2) / 3.0f;
						FVector LocalCentroid = VisTransform.InverseTransformPosition(Centroid);
						FVector2D Centroid2D(LocalCentroid.X, LocalCentroid.Y);

						bool bIsInsideDriveway = false;
						for (FHDMapLineRow* DwRow : LocalDriveways)
						{
							if (IsPointInPolygon2D(Centroid2D, DwRow->Points))
							{
								bIsInsideDriveway = true;
								break;
							}
						}

						// 면 내부에 있지 않은 삼각형은 도로 외곽선 밖의 노이즈이므로 즉시 소거
						if (!bIsInsideDriveway)
						{
							continue;
						}
					}

					// 2. 수직 에지 고도차 필터 (지상/지하 수직 벽 결합 제거)
					float MaxVerticalDiff = 350.f; // 3.5m 임계치
					if (FMath::Abs(V0.Z - V1.Z) > MaxVerticalDiff ||
						FMath::Abs(V1.Z - V2.Z) > MaxVerticalDiff ||
						FMath::Abs(V2.Z - V0.Z) > MaxVerticalDiff)
					{
						continue;
					}

					// 3. 에지 길이 필터 (동일 링크 뼈대 보호)
					bool bIsSameLink01 = (!LInfo0.SourceID.IsEmpty() && LInfo0.SourceID.Equals(LInfo1.SourceID));
					bool bIsSameLink12 = (!LInfo1.SourceID.IsEmpty() && LInfo1.SourceID.Equals(LInfo2.SourceID));
					bool bIsSameLink20 = (!LInfo2.SourceID.IsEmpty() && LInfo2.SourceID.Equals(LInfo0.SourceID));

					float L0 = FVector::Dist2D(V0, V1);
					float L1 = FVector::Dist2D(V1, V2);
					float L2 = FVector::Dist2D(V2, V0);

					if ((!bIsSameLink01 && L0 > MaxEdgeLength) ||
						(!bIsSameLink12 && L1 > MaxEdgeLength) ||
						(!bIsSameLink20 && L2 > MaxEdgeLength))
					{
						continue;
					}

					// 4. 내각 필터
					if (MinAngleDegree > 0.f)
					{
						FVector E0_Normal = (V1 - V0).GetSafeNormal();
						FVector E1_Normal = (V2 - V1).GetSafeNormal();
						FVector E2_Normal = (V0 - V2).GetSafeNormal();

						float CosA = FVector::DotProduct(E0_Normal, -E2_Normal);
						float CosB = FVector::DotProduct(E1_Normal, -E0_Normal);
						float CosC = FVector::DotProduct(E2_Normal, -E1_Normal);

						float AngleA = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosA, -1.f, 1.f)));
						float AngleB = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosB, -1.f, 1.f)));
						float AngleC = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosC, -1.f, 1.f)));

						if (AngleA < MinAngleDegree || AngleB < MinAngleDegree || AngleC < MinAngleDegree)
						{
							continue;
						}
					}

					// Winding Order 정렬
					FVector E0 = V1 - V0;
					FVector E1 = V2 - V0;
					FVector CrossVal = FVector::CrossProduct(E0, E1);

					int32 FinalY = Tri.Y;
					int32 FinalZ = Tri.Z;
					if (CrossVal.Z > 0.f)
					{
						FinalY = Tri.Z;
						FinalZ = Tri.Y;
					}

					int32 G_X = LocalToGlobalIndexMap[Tri.X];
					int32 G_Y = LocalToGlobalIndexMap[FinalY];
					int32 G_Z = LocalToGlobalIndexMap[FinalZ];

					if (G_X != G_Y && G_Y != G_Z && G_Z != G_X)
					{
						GlobalTriangles.Add(FIntVector(G_X, G_Y, G_Z));
					}
				}
			}
		}
		}
	}
	else
	{
		// 2. 이웃 링크 중심선 중 공간적으로 가장 가까운 점 좌표를 탐색하여 반환하는 헬퍼 람다
		auto GetClosestCenterlinePt = [&](const FVector& CurrentPt, const FString& NeighborLinkID) -> FVector
		{
			FHDMapA2LinkRow* NeighborRow = LinkRowMap.FindRef(NeighborLinkID);
			if (NeighborRow && NeighborRow->Points.Num() > 0)
			{
				FVector BestPt = NeighborRow->Points[0];
				float MinDistSqr = FLT_MAX;
				for (const FVector& Pt : NeighborRow->Points)
				{
					float DistSqr = FVector::DistSquared(CurrentPt, Pt);
					if (DistSqr < MinDistSqr)
					{
						MinDistSqr = DistSqr;
						BestPt = Pt;
					}
				}
				return BestPt;
			}
			return FVector::ZeroVector;
		};


	// 3. 이웃 링크의 생성된 정점들 중 인덱스 비율에 비례하는 정점 인덱스를 가져오는 헬퍼 람다 (꼬임 절대 방지)
	auto GetRatioMatchedVertexIdx = [&](int32 CurrentIdx, int32 CurrentNumPts, const FString& NeighborLinkID, bool bIsLeft) -> int32
	{
		const TArray<int32>* NeighborIndices = bIsLeft ? LinkRightVertexIndices.Find(NeighborLinkID) : LinkLeftVertexIndices.Find(NeighborLinkID);
		if (!NeighborIndices || NeighborIndices->Num() == 0)
		{
			return -1;
		}

		int32 NeighborNumPts = NeighborIndices->Num();
		if (CurrentNumPts < 2 || NeighborNumPts < 2)
		{
			return (*NeighborIndices)[CurrentIdx == 0 ? 0 : NeighborNumPts - 1];
		}

		// 인덱스 비율 계산 (0.0 ~ 1.0)
		float Ratio = (float)CurrentIdx / (float)(CurrentNumPts - 1);
		// 이웃 인덱스를 비율에 비례하도록 클램핑 매핑
		int32 TargetIdx = FMath::Clamp(FMath::RoundToInt(Ratio * (NeighborNumPts - 1)), 0, NeighborNumPts - 1);

		if (NeighborIndices->IsValidIndex(TargetIdx))
		{
			return (*NeighborIndices)[TargetIdx];
		}
		return -1;
	};

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Generating ribbon road mesh for %d links. Width: %.1f"), TargetRows.Num(), DefaultRoadWidth);

	for (FHDMapA2LinkRow* Row : TargetRows)
	{
		if (!Row || Row->ID.Equals(TEXT("ORIGIN"))) continue;
		const TArray<FVector>& Points = Row->Points;
		int32 NumPts = Points.Num();
		if (NumPts < 2) continue;

		TArray<int32> CurrentLinkLeftIndices;
		TArray<int32> CurrentLinkRightIndices;
		CurrentLinkLeftIndices.Reserve(NumPts);
		CurrentLinkRightIndices.Reserve(NumPts);

		FString CurrentLinkID = Row->ID;

		for (int32 i = 0; i < NumPts; ++i)
		{
			FVector Direction;
			if (i == 0)
			{
				Direction = Points[1] - Points[0];
			}
			else if (i == NumPts - 1)
			{
				Direction = Points[NumPts - 1] - Points[NumPts - 2];
			}
			else
			{
				Direction = Points[i + 1] - Points[i - 1];
			}

			Direction.Z = 0.0f; // 2D 평면 투영 방향 계산
			Direction.Normalize();

			// 도로 진행방향의 수직 법선 구하기 (Z Up 과 외적)
			FVector RightNormal = FVector(-Direction.Y, Direction.X, 0.0f);
			FVector CurrentPt = Points[i];

			// A. 왼쪽 정점 구하기 (L_LinkID 위상 매핑 및 중심선 보간)
			FVector LeftPtWorld;
			int32 LeftIdx = -1;

			if (!Row->L_LinkID.IsEmpty() && LinkRowMap.Contains(Row->L_LinkID))
			{
				FVector NeighborPt = GetClosestCenterlinePt(CurrentPt, Row->L_LinkID);
				FVector MidPt = (CurrentPt + NeighborPt) * 0.5f;
				FVector ExpectedPtWorld = VisTransform.TransformPosition(MidPt);
				ExpectedPtWorld.Z += ZOffset;

				// 이웃이 이미 등록한 우측 정점들 중 비율상 매칭되는 정점이 있는지 확인
				LeftIdx = GetRatioMatchedVertexIdx(i, NumPts, Row->L_LinkID, true);

				if (LeftIdx == -1)
				{
					LeftPtWorld = ExpectedPtWorld;
				}
			}
			else
			{
				// 이웃 도로가 없으므로 기존 방식대로 왼쪽 방향 충분히 연장 (HalfWidth = 3m)
				FVector LeftPtLocal = CurrentPt - RightNormal * HalfWidth;
				LeftPtWorld = VisTransform.TransformPosition(LeftPtLocal);
				LeftPtWorld.Z += ZOffset;
			}

			if (LeftIdx == -1)
			{
				bool bIsInterface = (i == 0 || i == NumPts - 1);
				float CurrentWeldDist = bIsInterface ? (WeldDistance * 2.5f) : WeldDistance;
				LeftIdx = FindOrAddVertex(LeftPtWorld, CurrentWeldDist, CurrentLinkID);
			}

			// B. 오른쪽 정점 구하기 (R_LinkID 위상 매핑 및 중심선 보간)
			FVector RightPtWorld;
			int32 RightIdx = -1;

			if (!Row->R_LinkID.IsEmpty() && LinkRowMap.Contains(Row->R_LinkID))
			{
				FVector NeighborPt = GetClosestCenterlinePt(CurrentPt, Row->R_LinkID);
				FVector MidPt = (CurrentPt + NeighborPt) * 0.5f;
				FVector ExpectedPtWorld = VisTransform.TransformPosition(MidPt);
				ExpectedPtWorld.Z += ZOffset;

				// 이웃이 이미 등록한 좌측 정점들 중 비율상 매칭되는 정점이 있는지 확인
				RightIdx = GetRatioMatchedVertexIdx(i, NumPts, Row->R_LinkID, false);

				if (RightIdx == -1)
				{
					RightPtWorld = ExpectedPtWorld;
				}
			}
			else
			{
				// 이웃 도로가 없으므로 기존 방식대로 오른쪽 방향 충분히 연장 (HalfWidth = 3m)
				FVector RightPtLocal = CurrentPt + RightNormal * HalfWidth;
				RightPtWorld = VisTransform.TransformPosition(RightPtLocal);
				RightPtWorld.Z += ZOffset;
			}

			if (RightIdx == -1)
			{
				bool bIsInterface = (i == 0 || i == NumPts - 1);
				float CurrentWeldDist = bIsInterface ? (WeldDistance * 2.5f) : WeldDistance;
				RightIdx = FindOrAddVertex(RightPtWorld, CurrentWeldDist, CurrentLinkID);
			}

			CurrentLinkLeftIndices.Add(LeftIdx);
			CurrentLinkRightIndices.Add(RightIdx);
		}

		// 내 정점 인덱스들을 맵에 저장해 둠 (나중에 처리되는 이웃 링크가 참고하도록 캐싱)
		LinkLeftVertexIndices.Add(CurrentLinkID, CurrentLinkLeftIndices);
		LinkRightVertexIndices.Add(CurrentLinkID, CurrentLinkRightIndices);

		// 사각형 스트립(Triangle Strip) 인덱스 생성
		for (int32 i = 0; i < NumPts - 1; ++i)
		{
			int32 L0 = CurrentLinkLeftIndices[i];
			int32 R0 = CurrentLinkRightIndices[i];
			int32 L1 = CurrentLinkLeftIndices[i + 1];
			int32 R1 = CurrentLinkRightIndices[i + 1];

			// 퇴화 삼각형(Degenerate Triangle) 방지 필터링 추가
			if (L0 != L1 && L1 != R0 && R0 != L0)
			{
				GlobalTriangles.Add(FIntVector(L0, L1, R0));
			}
			if (R0 != L1 && L1 != R1 && R1 != R0)
			{
				GlobalTriangles.Add(FIntVector(R0, L1, R1));
			}
		}
	}
}

	if (GlobalVerticesInfo.Num() < 3 || GlobalTriangles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Failed to generate vertices or triangles. Global Vertices: %d, Global Triangles: %d"), GlobalVerticesInfo.Num(), GlobalTriangles.Num());
		return;
	}

	// 4. DynamicMesh 컴포넌트에 최종 지오메트리 빌드
	UDynamicMeshComponent* DynMeshComp = OutputDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] OutputDynamicMeshActor does not have a DynamicMeshComponent."));
		return;
	}

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh) return;

	UE::Geometry::FDynamicMesh3 NativeMesh;
	NativeMesh.EnableAttributes();

	// 정점 어펜드
	TArray<int32> VertexIDs;
	for (const FGlobalVertexInfo& VInfo : GlobalVerticesInfo)
	{
		int32 VId = NativeMesh.AppendVertex(VInfo.Position);
		VertexIDs.Add(VId);
	}

	// 삼각형 어펜드
	for (const FIntVector& Tri : GlobalTriangles)
	{
		NativeMesh.AppendTriangle(VertexIDs[Tri.X], VertexIDs[Tri.Y], VertexIDs[Tri.Z]);
	}

	// 컴포넌트에 로드 및 갱신 통지
	DynMesh->SetMesh(MoveTemp(NativeMesh));

	// --- B3 Stamp (Direct Z-Snapped Append) 각인 처리 ---
	if (bEnableB3Stamping && VisualizerActor)
	{
		UDataTable* B3MarkTable = VisualizerActor->DT_B3_Mark;
		if (B3MarkTable)
		{
			TArray<FHDMapB3MarkRow*> B3Rows;
			B3MarkTable->GetAllRows<FHDMapB3MarkRow>(TEXT("HDMapMeshGen_B3Stamp"), B3Rows);
			
			UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Processing %d B3 Marks for stamping..."), B3Rows.Num());

			UE::Geometry::FDynamicMesh3* NativeMeshPtr = DynMesh->GetMeshPtr();
			if (NativeMeshPtr)
			{
				NativeMeshPtr->EnableAttributes();
				if (!NativeMeshPtr->Attributes()->GetMaterialID())
				{
					NativeMeshPtr->Attributes()->EnableMaterialID();
				}
				auto* MaterialIDs = NativeMeshPtr->Attributes()->GetMaterialID();

				FTransform TargetActorTransform = OutputDynamicMeshActor->GetActorTransform();

				for (FHDMapB3MarkRow* Row : B3Rows)
				{
					if (!Row || Row->ID.Equals(TEXT("ORIGIN")) || Row->Points.Num() < 3) continue;

					// ⚠️ [정차금지대 524] 스킵 필터링
					if (Row->Kind.Equals(TEXT("524"))) continue;

					// 1. 월드 좌표 및 월드 중심점 계산
					TArray<FVector> WorldPoints;
					FVector WorldCentroid = FVector::ZeroVector;
					for (const FVector& Pt : Row->Points)
					{
						FVector WP = VisTransform.TransformPosition(Pt);
						WorldPoints.Add(WP);
						WorldCentroid += WP;
					}
					WorldCentroid /= (float)WorldPoints.Num();

					float RoadZ = WorldCentroid.Z;
					if (bSnapToLandscape)
					{
						RoadZ = GetLandscapeZ(WorldCentroid) + ZOffset;
					}

					// 2. TargetActor의 로컬 좌표계로 다각형 점들을 변환
					FVector LocalCentroid = TargetActorTransform.InverseTransformPosition(FVector(WorldCentroid.X, WorldCentroid.Y, RoadZ));

					TArray<FVector2D> Polygon2D;
					for (const FVector& WP : WorldPoints)
					{
						FVector LocalWP = TargetActorTransform.InverseTransformPosition(FVector(WP.X, WP.Y, RoadZ));
						Polygon2D.Add(FVector2D(LocalWP.X - LocalCentroid.X, LocalWP.Y - LocalCentroid.Y));
					}

					// 마지막 중복 정점 제거
					if (Polygon2D.Num() > 1 && FVector2D::DistSquared(Polygon2D[0], Polygon2D.Last()) < 1.0f)
					{
						Polygon2D.RemoveAt(Polygon2D.Num() - 1);
					}

					if (Polygon2D.Num() < 3) continue;

					// Winding Order 판정 및 강제 CCW 변환
					float SignedArea = 0.0f;
					int32 NumPts = Polygon2D.Num();
					for (int32 i = 0; i < NumPts; ++i)
					{
						const FVector2D& P1 = Polygon2D[i];
						const FVector2D& P2 = Polygon2D[(i + 1) % NumPts];
						SignedArea += (P1.X * P2.Y - P2.X * P1.Y);
					}
					if (SignedArea < 0.0f)
					{
						TArray<FVector2D> Reversed;
						Reversed.Reserve(Polygon2D.Num());
						for (int32 idx = Polygon2D.Num() - 1; idx >= 0; --idx)
						{
							Reversed.Add(Polygon2D[idx]);
						}
						Polygon2D = Reversed;
					}

					// 3. 도로 곡률 고도(Z) 획득 및 정점 로컬 변환
					TArray<int32> VertexIndices;
					VertexIndices.Reserve(WorldPoints.Num());
					TArray<FVector> LocalPoints;
					LocalPoints.Reserve(WorldPoints.Num());

					for (int32 idx = 0; idx < WorldPoints.Num(); ++idx)
					{
						FVector WP = WorldPoints[idx];
						
						// 도로의 정확한 Z 높이를 해당 XY 좌표에서 획득하여 밀착시킴
						float SnapZ = GetRoadMeshZ(DynMesh, FVector2D(WP.X, WP.Y), WP.Z, TargetActorTransform);
						WP.Z = SnapZ + B3StampHeight; // 미세 오프셋 반영

						FVector LocalPt = TargetActorTransform.InverseTransformPosition(WP);
						LocalPoints.Add(LocalPt);

						int32 VIdx = NativeMeshPtr->AppendVertex(LocalPt);
						VertexIndices.Add(VIdx);
					}

					// 4. 2D 델로네 삼각분할 수행 및 삼각형 직접 주입
					TArray<FIntVector> Triangles2D;
					if (RunDelaunayTriangulation(LocalPoints, Triangles2D))
					{
						for (const FIntVector& Tri : Triangles2D)
						{
							FVector V0 = LocalPoints[Tri.X];
							FVector V1 = LocalPoints[Tri.Y];
							FVector V2 = LocalPoints[Tri.Z];

							FVector TriCentroid = (V0 + V1 + V2) / 3.0f;
							FVector2D Centroid2D(TriCentroid.X, TriCentroid.Y);

							if (IsPointInPolygon2D(Centroid2D, LocalPoints))
							{
								FVector E0 = V1 - V0;
								FVector E1 = V2 - V0;
								FVector CrossVal = FVector::CrossProduct(E0, E1);

								int32 FinalY = Tri.Y;
								int32 FinalZ = Tri.Z;
								if (CrossVal.Z > 0.f)
								{
									FinalY = Tri.Z;
									FinalZ = Tri.Y;
								}

								int32 G_V0 = VertexIndices[Tri.X];
								int32 G_V1 = VertexIndices[FinalY];
								int32 G_V2 = VertexIndices[FinalZ];

								if (G_V0 != G_V1 && G_V1 != G_V2 && G_V2 != G_V0)
								{
									int32 NewTri = NativeMeshPtr->AppendTriangle(G_V0, G_V1, G_V2);
									if (NewTri >= 0 && MaterialIDs)
									{
										MaterialIDs->SetValue(NewTri, 1); // 노면 표시 머티리얼 ID 할당
									}
								}
							}
						}
					}
				}
			}
		}
	}

	DynMeshComp->NotifyMeshUpdated();

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Extruded ribbon road mesh generated successfully! Vertices: %d, Triangles: %d"),
		GlobalVerticesInfo.Num(), GlobalTriangles.Num());
}


void AHDMapMeshGenerator::SaveToStaticMeshAsset()
{
	if (!OutputDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: OutputDynamicMeshActor is not specified."));
		return;
	}

	UDynamicMeshComponent* DynMeshComp = OutputDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp) return;

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh || DynMesh->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: DynamicMesh is empty. Please generate road mesh first."));
		return;
	}

	if (SaveAssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] SaveAssetPath is empty. Please define a path like /Game/Roads/SM_MyRoad."));
		return;
	}

	// 에셋 패키지명 및 에셋 이름 분리
	FString PackagePath = SaveAssetPath;
	FString AssetName;
	int32 LastSlashIndex;
	if (PackagePath.FindLastChar('/', LastSlashIndex))
	{
		AssetName = PackagePath.RightChop(LastSlashIndex + 1);
	}
	else
	{
		AssetName = PackagePath;
		PackagePath = TEXT("/Game/") + AssetName;
	}

	// 1. 패키지 생성 및 기존 파일 파괴/덮어쓰기 설정
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create package at %s"), *PackagePath);
		return;
	}
	Package->FullyLoad();

	// 2. 패키지 경로에서 UStaticMesh 에셋 객체를 얻거나 없으면 새로 생성
	UStaticMesh* TargetStaticMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *PackagePath));
	if (!TargetStaticMesh)
	{
		TargetStaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}

	if (!TargetStaticMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create or load StaticMesh object."));
		return;
	}

	// 3. GeometryScript API를 사용하여 Dynamic Mesh를 UStaticMesh 에셋으로 복사
	FGeometryScriptCopyMeshToAssetOptions CopyOptions;
	CopyOptions.bEnableRecomputeNormals = true;
	CopyOptions.bEnableRecomputeTangents = true;
	CopyOptions.bEnableRemoveDegenerates = true;
	FGeometryScriptMeshWriteLOD TargetLOD;
	TargetLOD.LODIndex = 0;

	EGeometryScriptOutcomePins Outcome;
	
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		DynMesh,
		TargetStaticMesh,
		CopyOptions,
		TargetLOD,
		Outcome
	);

	if (Outcome == EGeometryScriptOutcomePins::Success)
	{
		// 에셋 브라우저 및 데이터베이스에 에셋 알림 및 변경점 디스크 저장
		FAssetRegistryModule::AssetCreated(TargetStaticMesh);
		TargetStaticMesh->MarkPackageDirty();
		
		UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied and saved StaticMesh Asset at: %s"), *PackagePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to copy DynamicMesh to StaticMesh asset."));
	}
}

// 2D 델로네 삼각분할 외접원 판단 공식
bool IsPointInCircumcircle2D(const FVector& P, const FVector& A, const FVector& B, const FVector& C, FVector& OutCenter, float& OutRadiusSqr)
{
	float D = 2.f * (A.X * (B.Y - C.Y) + B.X * (C.Y - A.Y) + C.X * (A.Y - B.Y));
	
	// 세 점이 거의 일직선상에 있어 삼각형을 이룰 수 없는 경우
	if (FMath::Abs(D) < 1e-9f)
	{
		return false; 
	}

	float Ux = ((A.X * A.X + A.Y * A.Y) * (B.Y - C.Y) + (B.X * B.X + B.Y * B.Y) * (C.Y - A.Y) + (C.X * C.X + C.Y * C.Y) * (A.Y - B.Y)) / D;
	float Uy = ((A.X * A.X + A.Y * A.Y) * (C.X - B.X) + (B.X * B.X + B.Y * B.Y) * (A.X - C.X) + (C.X * C.X + C.Y * C.Y) * (B.X - A.X)) / D;

	OutCenter = FVector(Ux, Uy, 0.f);
	OutRadiusSqr = FVector::DistSquaredXY(A, OutCenter);

	float DistToPointSqr = FVector::DistSquaredXY(P, OutCenter);
	return DistToPointSqr <= OutRadiusSqr;
}

bool AHDMapMeshGenerator::RunDelaunayTriangulation(const TArray<FVector>& Vertices, TArray<FIntVector>& OutTriangles)
{
	int32 N = Vertices.Num();
	if (N < 3) return false;

	// 1. 전체 점군을 포함하는 거대한 '슈퍼 삼각형(Super-Triangle)'의 영역 계산
	float MinX = Vertices[0].X, MaxX = Vertices[0].X;
	float MinY = Vertices[0].Y, MaxY = Vertices[0].Y;

	for (const FVector& V : Vertices)
	{
		if (V.X < MinX) MinX = V.X;
		if (V.X > MaxX) MaxX = V.X;
		if (V.Y < MinY) MinY = V.Y;
		if (V.Y > MaxY) MaxY = V.Y;
	}

	float Dx = MaxX - MinX;
	float Dy = MaxY - MinY;
	float DeltaMax = FMath::Max(Dx, Dy);
	float MidX = (MinX + MaxX) * 0.5f;
	float MidY = (MinY + MaxY) * 0.5f;

	// 2. 수치 연산 정밀도(Float Precision) 확보를 위해 중심을 원점으로 이동하고 [-1.0, 1.0] 범위로 스케일링
	float ScaleFactor = DeltaMax > KINDA_SMALL_NUMBER ? DeltaMax : 1.f;
	TArray<FVector> TempVertices;
	TempVertices.Reserve(N + 3);
	for (int32 i = 0; i < N; ++i)
	{
		TempVertices.Add(FVector((Vertices[i].X - MidX) / ScaleFactor, (Vertices[i].Y - MidY) / ScaleFactor, 0.f));
	}

	// 로컬화된 정점들을 완벽하게 포함하는 거대 슈퍼 삼각형 정점 3개 추가 ([-100, 100] 범위 적용)
	int32 SuperV0 = TempVertices.Add(FVector(-100.f, -100.f, 0.f));
	int32 SuperV1 = TempVertices.Add(FVector(0.f, 100.f, 0.f));
	int32 SuperV2 = TempVertices.Add(FVector(100.f, -100.f, 0.f));

	// 2. 삼각형 리스트 생성 및 초기 슈퍼 삼각형 삽입
	TArray<FTriangle2D> TriangleList;
	TriangleList.Add(FTriangle2D(SuperV0, SuperV1, SuperV2));

	// 3. 점 하나씩 순차 삽입하며 Bowyer-Watson 알고리즘 진행
	for (int32 i = 0; i < N; ++i)
	{
		FVector P = TempVertices[i];
		TArray<FEdge2D> PolygonHole;

		// 3.1) 이 점 P가 외접원 내부에 속해 델로네 조건에 위배되는 기존 삼각형들 수집
		for (FTriangle2D& Tri : TriangleList)
		{
			FVector Center;
			float RadiusSqr;
			if (IsPointInCircumcircle2D(P, TempVertices[Tri.V0], TempVertices[Tri.V1], TempVertices[Tri.V2], Center, RadiusSqr))
			{
				Tri.bDead = true;
				// 위배되는 삼각형의 에지들을 PolygonHole에 저장
				PolygonHole.Add(FEdge2D(Tri.V0, Tri.V1));
				PolygonHole.Add(FEdge2D(Tri.V1, Tri.V2));
				PolygonHole.Add(FEdge2D(Tri.V2, Tri.V0));
			}
		}

		// 3.2) 중복 에지(위배 삼각형들 사이의 공유 내륙 에지)는 상쇄하고 고유 외곽선 에지만 필터링
		TArray<FEdge2D> UniqueEdges;
		for (int32 j = 0; j < PolygonHole.Num(); ++j)
		{
			bool bShared = false;
			for (int32 k = 0; k < PolygonHole.Num(); ++k)
			{
				if (j != k && PolygonHole[j] == PolygonHole[k])
				{
					bShared = true;
					break;
				}
			}
			if (!bShared)
			{
				UniqueEdges.Add(PolygonHole[j]);
			}
		}

		// 3.3) 위배된 파괴 삼각형들을 본 리스트에서 제거
		TriangleList.RemoveAll([](const FTriangle2D& T) { return T.bDead; });

		// 3.4) 고유 외곽 에지와 새로운 점 P를 연결하여 새로운 델로네 삼각형들 구성 후 리스트에 삽입
		for (const FEdge2D& Edge : UniqueEdges)
		{
			TriangleList.Add(FTriangle2D(Edge.V0, Edge.V1, i));
		}
	}

	// 4. 슈퍼 삼각형의 정점(SuperV0, SuperV1, SuperV2)을 포함하는 삼각형 최종 제거 후 출력 리스트 구축
	for (const FTriangle2D& Tri : TriangleList)
	{
		if (Tri.V0 == SuperV0 || Tri.V0 == SuperV1 || Tri.V0 == SuperV2 ||
			Tri.V1 == SuperV0 || Tri.V1 == SuperV1 || Tri.V1 == SuperV2 ||
			Tri.V2 == SuperV0 || Tri.V2 == SuperV1 || Tri.V2 == SuperV2)
		{
			continue;
		}

		OutTriangles.Add(FIntVector(Tri.V0, Tri.V1, Tri.V2));
	}

	return true;
}

float AHDMapMeshGenerator::GetDistance2D(const FVector& V1, const FVector& V2) const
{
	return FVector::Dist2D(V1, V2);
}

bool AHDMapMeshGenerator::IsPointInPolygon2D(const FVector2D& Point, const TArray<FVector>& PolygonPoints)
{
	int32 NumPoints = PolygonPoints.Num();
	if (NumPoints < 3) return false;

	// 1차 바운딩 박스 검사로 고속 필터링
	float MinX = PolygonPoints[0].X;
	float MaxX = PolygonPoints[0].X;
	float MinY = PolygonPoints[0].Y;
	float MaxY = PolygonPoints[0].Y;

	for (int32 i = 1; i < NumPoints; ++i)
	{
		const FVector& V = PolygonPoints[i];
		if (V.X < MinX) MinX = V.X;
		if (V.X > MaxX) MaxX = V.X;
		if (V.Y < MinY) MinY = V.Y;
		if (V.Y > MaxY) MaxY = V.Y;
	}

	if (Point.X < MinX || Point.X > MaxX || Point.Y < MinY || Point.Y > MaxY)
	{
		return false;
	}

	// 2차 레이캐스팅 (Ray-crossing) 알고리즘
	bool bInside = false;
	for (int32 i = 0, j = NumPoints - 1; i < NumPoints; j = i++)
	{
		const FVector& Vi = PolygonPoints[i];
		const FVector& Vj = PolygonPoints[j];

		if (((Vi.Y > Point.Y) != (Vj.Y > Point.Y)) &&
			(Point.X < (Vj.X - Vi.X) * (Point.Y - Vi.Y) / (Vj.Y - Vi.Y + 1e-7f) + Vi.X))
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

float AHDMapMeshGenerator::GetLandscapeZ(const FVector& WorldPos)
{
	UWorld* World = GetWorld();
	if (!World) return WorldPos.Z;

	// ECC_WorldStatic 레이캐스팅 대신 Landscape Actor를 직접 순회하여
	// 건물/구조물을 완전히 배제하고 순수 지형 높이만 쿼리합니다.
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* LandscapeProxy = *It;
		if (!IsValid(LandscapeProxy))
		{
			continue;
		}

		TOptional<float> OptHeight = LandscapeProxy->GetHeightAtLocation(WorldPos);
		if (OptHeight.IsSet())
		{
			return OptHeight.GetValue();
		}
	}

	// 지형을 찾지 못한 경우 원래 Z값 반환
	return WorldPos.Z;
}

// ─────────────────────────────────────────────────────────────────────────────
// Landscape Carving
// ─────────────────────────────────────────────────────────────────────────────

float AHDMapMeshGenerator::ComputeDistToPolygon2D(const FVector2D& Point, const TArray<FVector>& PolyWorldPoints)
{
	float MinDist = FLT_MAX;
	const int32 N = PolyWorldPoints.Num();
	for (int32 i = 0; i < N; ++i)
	{
		const int32 j = (i + 1) % N;
		const FVector2D A(PolyWorldPoints[i].X, PolyWorldPoints[i].Y);
		const FVector2D B(PolyWorldPoints[j].X, PolyWorldPoints[j].Y);
		const FVector2D AB = B - A;
		const float ABLenSq = AB.SizeSquared();
		float Dist;
		if (ABLenSq < 1e-6f)
		{
			Dist = FVector2D::Distance(Point, A);
		}
		else
		{
			const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / ABLenSq, 0.f, 1.f);
			Dist = FVector2D::Distance(Point, A + T * AB);
		}
		MinDist = FMath::Min(MinDist, Dist);
	}
	return MinDist;
}

void AHDMapMeshGenerator::CarveLandscapeForRoads()
{
#if WITH_EDITOR
	if (!VisualizerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarveLandscape] VisualizerActor가 지정되지 않았습니다."));
		return;
	}

	UDataTable* DrivewayTable = VisualizerActor->HDMapDataTable;
	if (!DrivewayTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarveLandscape] HDMapDataTable이 비어있습니다."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	FTransform VisTransform = VisualizerActor->GetActorTransform();

	// ─── 1. 도로 다각형 수집 (터널 제외, ORIGIN 제외) ───────────────────────
	struct FRoadPolygon
	{
		TArray<FVector> WorldPoints; // 월드 좌표계 FVector (XY 평면 사용)
		float           TargetWorldZ; // 조각 목표 Z (다각형 평균 Z + CarveZOffset)
	};

	TArray<FHDMapLineRow*> AllDriveways;
	DrivewayTable->GetAllRows<FHDMapLineRow>(TEXT("CarveLandscape"), AllDriveways);

	TArray<FRoadPolygon> RoadPolygons;
	for (FHDMapLineRow* DwRow : AllDriveways)
	{
		if (!DwRow || DwRow->ID.Equals(TEXT("ORIGIN")) || DwRow->Points.Num() < 3) continue;
		if (TunnelRoadIDs.Contains(DwRow->ID)) continue; // 지하 도로는 제외

		FRoadPolygon Poly;
		float SumZ = 0.f;
		for (const FVector& Pt : DwRow->Points)
		{
			FVector WorldPt = VisTransform.TransformPosition(Pt);
			Poly.WorldPoints.Add(WorldPt);
			SumZ += WorldPt.Z;
		}
		// 다각형 꼭짓점 평균 Z를 목표 깊이로 사용
		Poly.TargetWorldZ = (SumZ / (float)DwRow->Points.Num()) + CarveZOffset;
		RoadPolygons.Add(MoveTemp(Poly));
	}

	if (RoadPolygons.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarveLandscape] 처리할 도로 다각형이 없습니다."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CarveLandscape] %d개 도로 다각형으로 Landscape 조각 시작..."), RoadPolygons.Num());

	int32 TotalModifiedVerts = 0;

	// ─── 2. 씬의 모든 Landscape Proxy 순회 ──────────────────────────────────
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* LandscapeProxy = *It;
		if (!IsValid(LandscapeProxy)) continue;

		ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
		if (!LandscapeInfo) continue;

		const FTransform LandscapeTransform = LandscapeProxy->GetActorTransform();
		const FVector    LandscapeScale     = LandscapeProxy->GetActorScale3D();
		const float      LandscapeOriginZ   = LandscapeTransform.GetLocation().Z;

		// Landscape vertex 전체 범위 획득
		int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
		if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CarveLandscape] '%s' Extents를 가져올 수 없습니다."), *LandscapeProxy->GetName());
			continue;
		}

		const int32 SizeX = MaxX - MinX + 1;
		const int32 SizeY = MaxY - MinY + 1;

		// 전체 높이맵 읽기
		TArray<uint16> HeightData;
		HeightData.AddUninitialized(SizeX * SizeY);
		{
			FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
			LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);
		}

		bool bModified = false;
		int32 LandscapeModifiedVerts = 0;

		// ─── 3. 각 도로 다각형 처리 ────────────────────────────────────────
		for (const FRoadPolygon& RoadPoly : RoadPolygons)
		{
			const TArray<FVector>& WorldPoly = RoadPoly.WorldPoints;

			// 다각형 AABB (Feather 반경 + 여백 포함 확장)
			FBox PolyWorldBox(EForceInit::ForceInit);
			for (const FVector& Pt : WorldPoly) PolyWorldBox += Pt;
			PolyWorldBox = PolyWorldBox.ExpandBy(CarveFeatherRadius + LandscapeScale.X);

			// AABB → Landscape vertex 인덱스 범위 변환
			auto WorldToVert = [&](const FVector& WPos, int32& OutX, int32& OutY)
			{
				const FVector Local = LandscapeTransform.InverseTransformPosition(WPos);
				OutX = FMath::RoundToInt(Local.X);
				OutY = FMath::RoundToInt(Local.Y);
			};

			int32 VMinX, VMinY, VMaxX, VMaxY;
			WorldToVert(PolyWorldBox.Min, VMinX, VMinY);
			WorldToVert(PolyWorldBox.Max, VMaxX, VMaxY);

			// Min/Max 순서 보정 (Landscape 회전 시 역전 가능)
			if (VMinX > VMaxX) Swap(VMinX, VMaxX);
			if (VMinY > VMaxY) Swap(VMinY, VMaxY);

			// Landscape 범위로 클램프
			VMinX = FMath::Clamp(VMinX, MinX, MaxX);
			VMinY = FMath::Clamp(VMinY, MinY, MaxY);
			VMaxX = FMath::Clamp(VMaxX, MinX, MaxX);
			VMaxY = FMath::Clamp(VMaxY, MinY, MaxY);

			// 목표 WorldZ → Landscape uint16 heightmap 값 변환
			// LocalZ = (WorldZ - LandscapeZ) / ScaleZ
			// HeightValue = LocalZ * 128 + 32768  (LANDSCAPE_ZSCALE = 1/128)
			const float   TargetLocalZ      = (RoadPoly.TargetWorldZ - LandscapeOriginZ) / LandscapeScale.Z;
			const uint16  TargetHeightValue = (uint16)FMath::Clamp(TargetLocalZ * 128.f + 32768.f, 0.f, 65535.f);

			// ─── 4. AABB 내 각 vertex 처리 ─────────────────────────────────
			for (int32 VY = VMinY; VY <= VMaxY; ++VY)
			{
				for (int32 VX = VMinX; VX <= VMaxX; ++VX)
				{
					// vertex 월드 XY 위치
					const FVector VertWorldPos = LandscapeTransform.TransformPosition(
						FVector((float)VX, (float)VY, 0.f));
					const FVector2D VertXY(VertWorldPos.X, VertWorldPos.Y);

					// 다각형 내부 판정
					const bool bInside = IsPointInPolygon2D(VertXY, WorldPoly);
					float DistToEdge = 0.f;

					if (!bInside)
					{
						DistToEdge = ComputeDistToPolygon2D(VertXY, WorldPoly);
						if (DistToEdge > CarveFeatherRadius) continue; // Feather 범위 밖 → 스킵
					}

					// heightmap 배열 인덱스
					const int32 ArrX = VX - MinX;
					const int32 ArrY = VY - MinY;
					if (ArrX < 0 || ArrX >= SizeX || ArrY < 0 || ArrY >= SizeY) continue;

					const int32 Idx          = ArrY * SizeX + ArrX;
					const uint16 CurHeight   = HeightData[Idx];
					uint16       NewHeight;

					if (bInside)
					{
						// 내부: 목표 높이보다 낮을 때만 깎기 (기존이 이미 낮으면 건드리지 않음)
						NewHeight = FMath::Min(CurHeight, TargetHeightValue);
					}
					else
					{
						// Feather 경계: 0(경계선)~1(반경 끝) 블렌딩 → 자연스러운 경사
						const float  BlendAlpha = FMath::Clamp(DistToEdge / CarveFeatherRadius, 0.f, 1.f);
						const float  Blended    = FMath::Lerp((float)TargetHeightValue, (float)CurHeight, BlendAlpha);
						NewHeight = (uint16)FMath::Clamp(Blended, 0.f, 65535.f);
						// Feather 구간도 기존보다 높게 올리지는 않음
						NewHeight = FMath::Min(NewHeight, CurHeight);
					}

					if (NewHeight != CurHeight)
					{
						HeightData[Idx] = NewHeight;
						bModified = true;
						++LandscapeModifiedVerts;
					}
				}
			}
		} // end for RoadPolygons

		// ─── 5. 수정된 높이맵 적용 ───────────────────────────────────────────
		if (bModified)
		{
			FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
			LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);
			LandscapeProxy->MarkPackageDirty();
			LandscapeInfo->UpdateAllAddCollisions();
			TotalModifiedVerts += LandscapeModifiedVerts;
			UE_LOG(LogTemp, Log, TEXT("[CarveLandscape] '%s': %d개 vertex 수정됨."),
				*LandscapeProxy->GetName(), LandscapeModifiedVerts);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[CarveLandscape] '%s': 수정 대상 없음 (도로가 이미 지형보다 높거나 범위 밖)."),
				*LandscapeProxy->GetName());
		}

	} // end for ALandscapeProxy

	UE_LOG(LogTemp, Log, TEXT("[CarveLandscape] 완료 — 총 %d개 vertex 수정."), TotalModifiedVerts);

#else
	UE_LOG(LogTemp, Warning, TEXT("[CarveLandscape] 에디터 전용 기능입니다. 패키징된 빌드에서는 동작하지 않습니다."));
#endif
}

void AHDMapMeshGenerator::GenerateSidewalkMesh()
{
	if (!VisualizerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] VisualizerActor is not specified."));
		return;
	}

	if (!OutputSidewalkDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] OutputSidewalkDynamicMeshActor is not specified."));
		return;
	}

	OutputSidewalkDynamicMeshActor->SetActorTransform(FTransform::Identity);

	UDataTable* SidewalkTable = VisualizerActor->DT_Sidewalk;
	if (!SidewalkTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] DT_Sidewalk is not mapped."));
		return;
	}

	TArray<FHDMapSidewalkRow*> TargetRows;
	SidewalkTable->GetAllRows<FHDMapSidewalkRow>(TEXT("HDMapMeshGen_AllSidewalkSearch"), TargetRows);

	FTransform VisTransform = VisualizerActor->GetActorTransform();

	// ─── 도로 메쉬 고도 쿼리를 위한 캐싱 셋업 ───────────────────────────
	UDynamicMesh* RoadDynMesh = nullptr;
	FTransform SnapTransform = FTransform::Identity;
	UDynamicMesh* TempRoadDynMesh = nullptr; // GC 방지용 임시 라이프사이클 참조

	if (TargetRoadStaticMeshActor)
	{
		UStaticMeshComponent* SMComp = TargetRoadStaticMeshActor->GetStaticMeshComponent();
		if (SMComp && SMComp->GetStaticMesh())
		{
			UStaticMesh* RoadSM = SMComp->GetStaticMesh();
			TempRoadDynMesh = NewObject<UDynamicMesh>();
			
			FGeometryScriptCopyMeshFromAssetOptions CopyOptions;
			FGeometryScriptMeshReadLOD TargetLOD;
			TargetLOD.LODIndex = 0;
			EGeometryScriptOutcomePins Outcome;

			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				RoadSM,
				TempRoadDynMesh,
				CopyOptions,
				TargetLOD,
				Outcome
			);

			if (Outcome == EGeometryScriptOutcomePins::Success)
			{
				RoadDynMesh = TempRoadDynMesh;
				SnapTransform = TargetRoadStaticMeshActor->GetActorTransform();
				UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied static mesh '%s' for sidewalk snapping."), *RoadSM->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Failed to copy static mesh to temp dynamic mesh. Fallback to landscape."));
			}
		}
	}
	else if (OutputDynamicMeshActor)
	{
		UDynamicMeshComponent* DynMeshComp = OutputDynamicMeshActor->GetDynamicMeshComponent();
		if (DynMeshComp)
		{
			RoadDynMesh = DynMeshComp->GetDynamicMesh();
			SnapTransform = OutputDynamicMeshActor->GetActorTransform();
		}
	}

	// 전체 메쉬에 누적될 글로벌 정점 및 소속 링크 정보 구조체
	struct FGlobalVertexInfo
	{
		FVector Position;
		FString SourceID;
	};

	TArray<FGlobalVertexInfo> GlobalVerticesInfo;
	TArray<FIntVector> GlobalTriangles;

	auto FindOrAddVertex = [&](const FVector& NewVert, float MaxDistance, const FString& CurrentLinkID) -> int32
	{
		for (int32 Index = 0; Index < GlobalVerticesInfo.Num(); ++Index)
		{
			if (FVector::DistXY(GlobalVerticesInfo[Index].Position, NewVert) <= MaxDistance)
			{
				if (FMath::Abs(GlobalVerticesInfo[Index].Position.Z - NewVert.Z) < 5.0f)
				{
					return Index;
				}
			}
		}

		FGlobalVertexInfo NewInfo;
		NewInfo.Position = NewVert;
		NewInfo.SourceID = CurrentLinkID;
		return GlobalVerticesInfo.Add(NewInfo);
	};

	// 1. 다각형 데이터 경계 수집 (2D 결합을 위해 스냅은 보류하고 XY만 수집)
	TArray<TArray<FVector>> PolyBoundaries;
	TArray<FHDMapSidewalkRow*> PolyRows;

	for (FHDMapSidewalkRow* DwRow : TargetRows)
	{
		if (!DwRow || DwRow->UFID.Equals(TEXT("ORIGIN")) || DwRow->Points.Num() < 3) continue;

		TArray<FVector> LocalVertices;
		LocalVertices.Reserve(DwRow->Points.Num());
		for (const FVector& Pt : DwRow->Points)
		{
			FVector PtWorld = VisTransform.TransformPosition(Pt);
			PtWorld.Z = 0.0f; // 2D 결합을 위해 일단 높이를 0으로 초기화
			LocalVertices.Add(PtWorld);
		}
		PolyBoundaries.Add(MoveTemp(LocalVertices));
		PolyRows.Add(DwRow);
	}

	// 2. 글로벌 경계 정점 병합 (Disjoint Set Weld - 2D 평면 결합, 원본 정점 대상)
	struct FGlobalVertexRef
	{
		int32 PolyIdx;
		int32 VertIdx;
		FVector* ValuePtr;
	};

	TArray<FGlobalVertexRef> GlobalVerts;
	for (int32 PolyIdx = 0; PolyIdx < PolyBoundaries.Num(); ++PolyIdx)
	{
		for (int32 VertIdx = 0; VertIdx < PolyBoundaries[PolyIdx].Num(); ++VertIdx)
		{
			GlobalVerts.Add({ PolyIdx, VertIdx, &PolyBoundaries[PolyIdx][VertIdx] });
		}
	}

	const int32 TotalVerts = GlobalVerts.Num();
	if (TotalVerts > 0)
	{
		FDisjointSet DS(TotalVerts);
		for (int32 i = 0; i < TotalVerts; ++i)
		{
			for (int32 j = i + 1; j < TotalVerts; ++j)
			{
				if (GlobalVerts[i].PolyIdx == GlobalVerts[j].PolyIdx) continue;

				if (FVector::DistXY(*GlobalVerts[i].ValuePtr, *GlobalVerts[j].ValuePtr) < WeldDistance)
				{
					DS.Union(i, j);
				}
			}
		}

		TArray<FVector> GroupSum;
		TArray<int32> GroupCount;
		GroupSum.AddZeroed(TotalVerts);
		GroupCount.AddZeroed(TotalVerts);

		for (int32 i = 0; i < TotalVerts; ++i)
		{
			int32 Root = DS.Find(i);
			GroupSum[Root] += *GlobalVerts[i].ValuePtr;
			GroupCount[Root]++;
		}

		for (int32 i = 0; i < TotalVerts; ++i)
		{
			int32 Root = DS.Find(i);
			FVector AvgPt = GroupSum[Root] / (float)GroupCount[Root];
			*GlobalVerts[i].ValuePtr = AvgPt;
		}
	}

	// 3. 경계 정점 중복 제거
	for (TArray<FVector>& Boundary : PolyBoundaries)
	{
		for (int32 i = Boundary.Num() - 1; i > 0; --i)
		{
			if (FVector::DistXY(Boundary[i], Boundary[i - 1]) < WeldDistance)
			{
				Boundary.RemoveAt(i);
			}
		}
		if (Boundary.Num() > 1 && FVector::DistXY(Boundary[0], Boundary.Last()) < WeldDistance)
		{
			Boundary.RemoveAt(Boundary.Num() - 1);
		}
	}

	// 4. 에지 분할 (Edge Subdivision - Weld 및 중복 제거 완료 후 사후 분할)
	if (EdgeSubdivisionCount > 1)
	{
		TArray<TArray<FVector>> SubdividedBoundaries;
		SubdividedBoundaries.Reserve(PolyBoundaries.Num());
		for (const TArray<FVector>& Boundary : PolyBoundaries)
		{
			TArray<FVector> NewBoundary;
			const int32 NumPts = Boundary.Num();
			if (NumPts < 3)
			{
				SubdividedBoundaries.Add(Boundary);
				continue;
			}
			NewBoundary.Reserve(NumPts * EdgeSubdivisionCount);
			for (int32 i = 0; i < NumPts; ++i)
			{
				const FVector& V0 = Boundary[i];
				const FVector& V1 = Boundary[(i + 1) % NumPts];
				NewBoundary.Add(V0);
				for (int32 Step = 1; Step < EdgeSubdivisionCount; ++Step)
				{
					float T = (float)Step / (float)EdgeSubdivisionCount;
					FVector SubdivPt = FMath::Lerp(V0, V1, T);
					SubdivPt.Z = 0.0f;
					NewBoundary.Add(SubdivPt);
				}
			}
			SubdividedBoundaries.Add(MoveTemp(NewBoundary));
		}
		PolyBoundaries = MoveTemp(SubdividedBoundaries);
	}

	// 4.5) 2D 결합 및 분할 완료된 정점들을 지형 높이 및 도로 메쉬에 스냅 (틈새 및 파묻힘 원천 차단)
	if (bSnapToLandscape)
	{
		for (TArray<FVector>& Boundary : PolyBoundaries)
		{
			for (FVector& Pt : Boundary)
			{
				float LandZ = GetLandscapeZ(Pt);
				float SnapZ = GetRoadMeshZ(RoadDynMesh, FVector2D(Pt.X, Pt.Y), LandZ, SnapTransform);
				Pt.Z = SnapZ;
			}
		}
	}

	// 4.6) 다각형 간 공유하는 에지 수집 및 판별 (수직 측벽 꼬임 해결)
	auto GetEdgeKey = [](int32 V0, int32 V1) -> uint64
	{
		int32 MinV = FMath::Min(V0, V1);
		int32 MaxV = FMath::Max(V0, V1);
		return ((uint64)MinV << 32) | (uint32)MaxV;
	};

	TArray<TArray<int32>> PolyBottomIndices;
	TArray<TArray<int32>> PolyTopIndices;
	PolyBottomIndices.AddDefaulted(PolyBoundaries.Num());
	PolyTopIndices.AddDefaulted(PolyBoundaries.Num());

	// 정점 인덱스를 전 다각형에 대해 일괄적으로 미리 확보
	for (int32 PolyIdx = 0; PolyIdx < PolyBoundaries.Num(); ++PolyIdx)
	{
		TArray<FVector>& BoundaryPoints = PolyBoundaries[PolyIdx];
		int32 BoundaryCount = BoundaryPoints.Num();

		PolyBottomIndices[PolyIdx].AddUninitialized(BoundaryCount);
		PolyTopIndices[PolyIdx].AddUninitialized(BoundaryCount);

		FHDMapSidewalkRow* DwRow = PolyRows[PolyIdx];

		for (int32 v = 0; v < BoundaryCount; ++v)
		{
			FVector BasePos = BoundaryPoints[v];
			
			FVector BottomPos = BasePos;
			BottomPos.Z += SidewalkZOffset;
			PolyBottomIndices[PolyIdx][v] = FindOrAddVertex(BottomPos, WeldDistance, DwRow->UFID);

			FVector TopPos = BasePos;
			TopPos.Z += (SidewalkZOffset + SidewalkHeight);
			PolyTopIndices[PolyIdx][v] = FindOrAddVertex(TopPos, WeldDistance, DwRow->UFID);
		}
	}

	// 에지별 출현 횟수 카운팅
	TMap<uint64, int32> EdgeUsageMap;
	for (int32 PolyIdx = 0; PolyIdx < PolyBoundaries.Num(); ++PolyIdx)
	{
		int32 BoundaryCount = PolyBoundaries[PolyIdx].Num();
		if (BoundaryCount < 3) continue;

		const TArray<int32>& BottomIndices = PolyBottomIndices[PolyIdx];

		for (int32 i = 0; i < BoundaryCount; ++i)
		{
			int32 next_i = (i + 1) % BoundaryCount;
			int32 B0 = BottomIndices[i];
			int32 B1 = BottomIndices[next_i];

			uint64 EdgeKey = GetEdgeKey(B0, B1);
			EdgeUsageMap.FindOrAdd(EdgeKey, 0)++;
		}
	}

	// 5. 각 다각형별 메쉬 생성
	for (int32 PolyIdx = 0; PolyIdx < PolyBoundaries.Num(); ++PolyIdx)
	{
		TArray<FVector>& BoundaryPoints = PolyBoundaries[PolyIdx];
		if (BoundaryPoints.Num() < 3) continue;

		FHDMapSidewalkRow* DwRow = PolyRows[PolyIdx];
		TArray<FVector> SyncBoundary = BoundaryPoints;
		int32 BoundaryCount = SyncBoundary.Num();

		const TArray<int32>& BottomGlobalIndices = PolyBottomIndices[PolyIdx];
		const TArray<int32>& TopGlobalIndices = PolyTopIndices[PolyIdx];

		// 5.2) 그리드 세분화 (Grid Refinement)
		TArray<FVector> RefinedVertices = SyncBoundary;
		if (bSnapToLandscape && bEnableSidewalkGridRefinement && DwRow->Points.Num() >= 3)
		{
			FBox2D PolyBox(EForceInit::ForceInit);
			for (const FVector& Pt : DwRow->Points)
			{
				PolyBox += FVector2D(Pt.X, Pt.Y);
			}

			float LocalGridSpacing = FMath::Max(50.f, SidewalkGridSpacing);

			for (float GridX = PolyBox.Min.X + LocalGridSpacing; GridX < PolyBox.Max.X; GridX += LocalGridSpacing)
			{
				for (float GridY = PolyBox.Min.Y + LocalGridSpacing; GridY < PolyBox.Max.Y; GridY += LocalGridSpacing)
				{
					FVector2D TestPt(GridX, GridY);
					if (IsPointInPolygon2D(TestPt, DwRow->Points))
					{
						FVector PtLocal(GridX, GridY, 0.f);
						FVector PtWorld = VisTransform.TransformPosition(PtLocal);
						
						float LandZ = GetLandscapeZ(PtWorld);
						float SnapZ = GetRoadMeshZ(RoadDynMesh, FVector2D(PtWorld.X, PtWorld.Y), LandZ, SnapTransform);
						PtWorld.Z = SnapZ;

						RefinedVertices.Add(PtWorld);
					}
				}
			}
		}

		if (RefinedVertices.Num() < 3) continue;

		// 5.3) 상판(Top Face) 델로네 삼각분할
		TArray<FIntVector> RawTriangles;
		if (RunDelaunayTriangulation(RefinedVertices, RawTriangles))
		{
			TArray<int32> DelaunayGlobalIndices;
			DelaunayGlobalIndices.AddUninitialized(RefinedVertices.Num());
			
			// 경계 정점들은 이미 캐싱된 상판 인덱스를 매칭
			for (int32 v = 0; v < BoundaryCount; ++v)
			{
				DelaunayGlobalIndices[v] = TopGlobalIndices[v];
			}
			// 내부 그리드 정점들은 새로이 FindOrAddVertex 수행
			for (int32 v = BoundaryCount; v < RefinedVertices.Num(); ++v)
			{
				FVector TopPos = RefinedVertices[v];
				TopPos.Z += (SidewalkZOffset + SidewalkHeight);
				DelaunayGlobalIndices[v] = FindOrAddVertex(TopPos, WeldDistance, DwRow->UFID);
			}

			for (const FIntVector& Tri : RawTriangles)
			{
				FVector V0 = RefinedVertices[Tri.X];
				FVector V1 = RefinedVertices[Tri.Y];
				FVector V2 = RefinedVertices[Tri.Z];

				FVector Centroid = (V0 + V1 + V2) / 3.0f;
				FVector2D Centroid2D(Centroid.X, Centroid.Y);

				if (IsPointInPolygon2D(Centroid2D, SyncBoundary))
				{
					FVector E0 = V1 - V0;
					FVector E1 = V2 - V0;
					FVector CrossVal = FVector::CrossProduct(E0, E1);

					int32 FinalY = Tri.Y;
					int32 FinalZ = Tri.Z;
					if (CrossVal.Z > 0.f)
					{
						FinalY = Tri.Z;
						FinalZ = Tri.Y;
					}

					int32 G_X = DelaunayGlobalIndices[Tri.X];
					int32 G_Y = DelaunayGlobalIndices[FinalY];
					int32 G_Z = DelaunayGlobalIndices[FinalZ];

					if (G_X != G_Y && G_Y != G_Z && G_Z != G_X)
					{
						GlobalTriangles.Add(FIntVector(G_X, G_Y, G_Z));
					}
				}
			}
		}

		// 5.4) 측벽(Side Wall) 생성 (1:1 정렬된 매칭 인덱스 상속)
		if (BoundaryCount >= 3)
		{
			for (int32 i = 0; i < BoundaryCount; ++i)
			{
				int32 next_i = (i + 1) % BoundaryCount;

				int32 B0 = BottomGlobalIndices[i];
				int32 B1 = BottomGlobalIndices[next_i];
				int32 T0 = TopGlobalIndices[i];
				int32 T1 = TopGlobalIndices[next_i];

				// 공유 에지 필터링 (맞닿은 이웃 다각형이 공유하는 내부 경계는 측벽 생성을 제외)
				uint64 EdgeKey = GetEdgeKey(B0, B1);
				if (EdgeUsageMap.FindRef(EdgeKey) > 1)
				{
					continue;
				}

				if (B0 != B1 && B1 != T1 && T1 != B0)
				{
					GlobalTriangles.Add(FIntVector(B0, B1, T1));
				}
				if (B0 != T1 && T1 != T0 && T0 != B0)
				{
					GlobalTriangles.Add(FIntVector(B0, T1, T0));
				}
			}
		}
	}

	if (GlobalVerticesInfo.Num() < 3 || GlobalTriangles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Failed to generate sidewalk vertices or triangles. Vertices: %d, Triangles: %d"), GlobalVerticesInfo.Num(), GlobalTriangles.Num());
		return;
	}

	// 6. DynamicMesh 컴포넌트에 최종 지오메트리 빌드
	UDynamicMeshComponent* DynMeshComp = OutputSidewalkDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] OutputSidewalkDynamicMeshActor does not have a DynamicMeshComponent."));
		return;
	}

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh) return;

	UE::Geometry::FDynamicMesh3 NativeMesh;
	NativeMesh.EnableAttributes();

	TArray<int32> VertexIDs;
	for (const FGlobalVertexInfo& VInfo : GlobalVerticesInfo)
	{
		int32 VId = NativeMesh.AppendVertex(VInfo.Position);
		VertexIDs.Add(VId);
	}

	for (const FIntVector& Tri : GlobalTriangles)
	{
		NativeMesh.AppendTriangle(VertexIDs[Tri.X], VertexIDs[Tri.Y], VertexIDs[Tri.Z]);
	}

	DynMesh->SetMesh(MoveTemp(NativeMesh));
	DynMeshComp->NotifyMeshUpdated();

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Sidewalk 3D mesh generated successfully! Vertices: %d, Triangles: %d"),
		GlobalVerticesInfo.Num(), GlobalTriangles.Num());
}

void AHDMapMeshGenerator::SaveSidewalkToStaticMeshAsset()
{
	if (!OutputSidewalkDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: OutputSidewalkDynamicMeshActor is not specified."));
		return;
	}

	UDynamicMeshComponent* DynMeshComp = OutputSidewalkDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp) return;

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh || DynMesh->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: DynamicMesh is empty. Please generate sidewalk mesh first."));
		return;
	}

	if (SaveSidewalkAssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] SaveSidewalkAssetPath is empty. Please define a path like /Game/Roads/SM_MySidewalk."));
		return;
	}

	FString PackagePath = SaveSidewalkAssetPath;
	FString AssetName;
	int32 LastSlashIndex;
	if (PackagePath.FindLastChar('/', LastSlashIndex))
	{
		AssetName = PackagePath.RightChop(LastSlashIndex + 1);
	}
	else
	{
		AssetName = PackagePath;
		PackagePath = TEXT("/Game/") + AssetName;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create package at %s"), *PackagePath);
		return;
	}
	Package->FullyLoad();

	UStaticMesh* TargetStaticMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *PackagePath));
	if (!TargetStaticMesh)
	{
		TargetStaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}

	if (!TargetStaticMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create or load StaticMesh object."));
		return;
	}

	FGeometryScriptCopyMeshToAssetOptions CopyOptions;
	CopyOptions.bEnableRecomputeNormals = true;
	CopyOptions.bEnableRecomputeTangents = true;
	CopyOptions.bEnableRemoveDegenerates = true;
	FGeometryScriptMeshWriteLOD TargetLOD;
	TargetLOD.LODIndex = 0;

	EGeometryScriptOutcomePins Outcome;
	
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		DynMesh,
		TargetStaticMesh,
		CopyOptions,
		TargetLOD,
		Outcome
	);

	if (Outcome == EGeometryScriptOutcomePins::Success)
	{
		FAssetRegistryModule::AssetCreated(TargetStaticMesh);
		TargetStaticMesh->MarkPackageDirty();
		
		UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied and saved StaticMesh Asset at: %s"), *PackagePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to copy DynamicMesh to StaticMesh asset."));
	}
}

static bool GetBarycentricCoords2D(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C, float& OutU, float& OutV, float& OutW)
{
	FVector2D v0 = B - A;
	FVector2D v1 = C - A;
	FVector2D v2 = P - A;

	float den = v0.X * v1.Y - v1.X * v0.Y;
	if (FMath::Abs(den) < 1e-6f) return false;

	float v = (v2.X * v1.Y - v1.X * v2.Y) / den;
	float w = (v0.X * v2.Y - v2.X * v0.Y) / den;
	float u = 1.0f - v - w;

	OutU = u;
	OutV = v;
	OutW = w;

	// 경계선 오차 방지: 외측 5% 마진 허용 (-0.05 ~ 1.05)
	return (u >= -0.05f && v >= -0.05f && w >= -0.05f && u <= 1.05f && v <= 1.05f && w <= 1.05f);
}

float AHDMapMeshGenerator::GetRoadMeshZ(UDynamicMesh* RoadDynMesh, const FVector2D& Pt, float FallbackZ, const FTransform& ActorTransform)
{
	if (!RoadDynMesh || RoadDynMesh->IsEmpty()) return FallbackZ;

	float BestZ = -FLT_MAX;
	float MinZDiff = FLT_MAX;
	bool bFound = false;

	RoadDynMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
	{
		for (int32 TriID : Mesh.TriangleIndicesItr())
		{
			UE::Geometry::FIndex3i Tri = Mesh.GetTriangle(TriID);
			FVector A_Local = Mesh.GetVertex(Tri.A);
			FVector B_Local = Mesh.GetVertex(Tri.B);
			FVector C_Local = Mesh.GetVertex(Tri.C);

			// 월드 좌표계 변환
			FVector A = ActorTransform.TransformPosition(A_Local);
			FVector B = ActorTransform.TransformPosition(B_Local);
			FVector C = ActorTransform.TransformPosition(C_Local);

			float MinX = FMath::Min3(A.X, B.X, C.X);
			float MaxX = FMath::Max3(A.X, B.X, C.X);
			float MinY = FMath::Min3(A.Y, B.Y, C.Y);
			float MaxY = FMath::Max3(A.Y, B.Y, C.Y);

			// 바운딩 박스 검사에도 여유 마진 적용 (50cm)
			const float Margin = 50.f;
			if (Pt.X < MinX - Margin || Pt.X > MaxX + Margin || Pt.Y < MinY - Margin || Pt.Y > MaxY + Margin)
			{
				continue;
			}

			float U, V, W;
			if (GetBarycentricCoords2D(Pt, FVector2D(A.X, A.Y), FVector2D(B.X, B.Y), FVector2D(C.X, C.Y), U, V, W))
			{
				float TriZ = U * A.Z + V * B.Z + W * C.Z;
				if (TriZ > BestZ)
				{
					BestZ = TriZ;
					bFound = true;
				}
			}
		}
	});

	return bFound ? BestZ : FallbackZ;
}

void AHDMapMeshGenerator::GenerateLaneMesh()
{
	if (!VisualizerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] VisualizerActor is not specified."));
		return;
	}

	if (!OutputLaneDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] OutputLaneDynamicMeshActor is not specified."));
		return;
	}

	UDataTable* LaneTable = VisualizerActor->DT_B2_Lane;
	if (!LaneTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] DT_B2_Lane is not mapped in visualizer."));
		return;
	}

	// DynamicMeshComponent 및 DynamicMesh 준비
	UDynamicMeshComponent* DynMeshComp = OutputLaneDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] OutputLaneDynamicMeshActor does not have a DynamicMeshComponent."));
		return;
	}

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh) return;

	// 컴포넌트 머티리얼 세팅
	DynMeshComp->SetMaterial(0, WhiteMaterial);
	DynMeshComp->SetMaterial(1, YellowMaterial);
	DynMeshComp->SetMaterial(2, BlueMaterial);

	// 출력 Dynamic Mesh Actor 트랜스폼 Identity
	OutputLaneDynamicMeshActor->SetActorTransform(FTransform::Identity);
	FTransform VisTransform = VisualizerActor->GetActorTransform();

	// 도로 스냅을 위한 타겟 도로 메쉬 획득
	UDynamicMesh* RoadDynMesh = nullptr;
	FTransform RoadTransform = FTransform::Identity;
	UDynamicMesh* TempRoadDynMesh = nullptr; // GC 방지용 임시 라이프사이클 참조

	if (TargetRoadStaticMeshActor)
	{
		UStaticMeshComponent* SMComp = TargetRoadStaticMeshActor->GetStaticMeshComponent();
		if (SMComp && SMComp->GetStaticMesh())
		{
			UStaticMesh* RoadSM = SMComp->GetStaticMesh();
			TempRoadDynMesh = NewObject<UDynamicMesh>();
			
			FGeometryScriptCopyMeshFromAssetOptions CopyOptions;
			FGeometryScriptMeshReadLOD TargetLOD;
			TargetLOD.LODIndex = 0;
			EGeometryScriptOutcomePins Outcome;

			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				RoadSM,
				TempRoadDynMesh,
				CopyOptions,
				TargetLOD,
				Outcome
			);

			if (Outcome == EGeometryScriptOutcomePins::Success)
			{
				RoadDynMesh = TempRoadDynMesh;
				RoadTransform = TargetRoadStaticMeshActor->GetActorTransform();
				UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied static mesh '%s' for lane snapping."), *RoadSM->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Failed to copy static mesh to temp dynamic mesh for lane snapping. Fallback to landscape."));
			}
		}
	}
	else if (OutputDynamicMeshActor)
	{
		UDynamicMeshComponent* RoadDynMeshComp = OutputDynamicMeshActor->GetDynamicMeshComponent();
		if (RoadDynMeshComp)
		{
			RoadDynMesh = RoadDynMeshComp->GetDynamicMesh();
			RoadTransform = OutputDynamicMeshActor->GetActorTransform();
		}
	}

	// Native Mesh 생성용
	UE::Geometry::FDynamicMesh3 NativeMesh;
	NativeMesh.EnableAttributes();
	
	// 머티리얼 속성이 없으면 활성화
	if (!NativeMesh.Attributes()->GetMaterialID())
	{
		NativeMesh.Attributes()->EnableMaterialID();
	}
	auto* MaterialIDs = NativeMesh.Attributes()->GetMaterialID();

	TArray<FHDMapB2LaneRow*> TargetRows;
	LaneTable->GetAllRows<FHDMapB2LaneRow>(TEXT("HDMapMeshGen_AllB2Search"), TargetRows);

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Generating Lane Mesh for %d lanes..."), TargetRows.Num());

	// 포인트 리샘플링 헬퍼 람다
	auto ResamplePoints = [](const TArray<FVector>& OriginalPoints, float SampleDist, TArray<FVector>& OutResampled)
	{
		if (OriginalPoints.Num() < 2) return;

		OutResampled.Add(OriginalPoints[0]);
		float AccumulatedDistance = 0.f;
		float TargetDistance = SampleDist;

		for (int32 idx = 0; idx < OriginalPoints.Num() - 1; ++idx)
		{
			FVector P0 = OriginalPoints[idx];
			FVector P1 = OriginalPoints[idx + 1];
			float SegmentLen = FVector::Dist(P0, P1);

			while (AccumulatedDistance + SegmentLen >= TargetDistance)
			{
				float Ratio = (TargetDistance - AccumulatedDistance) / SegmentLen;
				FVector InterpolatedPt = FMath::Lerp(P0, P1, Ratio);
				OutResampled.Add(InterpolatedPt);
				TargetDistance += SampleDist;
			}

			AccumulatedDistance += SegmentLen;
		}

		if (FVector::Dist(OutResampled.Last(), OriginalPoints.Last()) > SampleDist * 0.1f)
		{
			OutResampled.Add(OriginalPoints.Last());
		}
	};

	// 각 개별 선 조각(Ribbon Strip)을 그리는 헬퍼 람다
	auto GenerateLaneStrip = [&](const TArray<FVector>& PathPoints, float StartOffset, float EndOffset, bool bDashed, float SolidLength, float SpaceLength, int32 MaterialID)
	{
		if (PathPoints.Num() < 2) return;

		float AccumulatedDistance = 0.f;
		float Period = SolidLength + SpaceLength;

		bool bLastWasSolid = false;
		int32 LastVIndexLeft = -1;
		int32 LastVIndexRight = -1;

		for (int32 i = 0; i < PathPoints.Num(); ++i)
		{
			FVector Curr = PathPoints[i];
			FVector Dir = FVector::ForwardVector;

			if (i < PathPoints.Num() - 1)
			{
				Dir = (PathPoints[i + 1] - Curr).GetSafeNormal2D();
			}
			else if (i > 0)
			{
				Dir = (Curr - PathPoints[i - 1]).GetSafeNormal2D();
			}

			FVector RightVec = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal2D();

			// 현재 지점까지의 거리 누적
			if (i > 0)
			{
				AccumulatedDistance += FVector::Dist(PathPoints[i], PathPoints[i - 1]);
			}

			// 점선 구간 검사
			bool bIsSolid = true;
			if (bDashed && Period > 0.0f)
			{
				float DistInPeriod = fmod(AccumulatedDistance, Period);
				bIsSolid = (DistInPeriod < SolidLength);
			}

			if (!bIsSolid)
			{
				bLastWasSolid = false;
				LastVIndexLeft = -1;
				LastVIndexRight = -1;
				continue;
			}

			// 정점 2개 (좌, 우) 생성
			FVector PtLeft = Curr + RightVec * StartOffset;
			FVector PtRight = Curr + RightVec * EndOffset;

			// Z 고도 정합 (월드 물리 레이캐스트 우선 ➡️ 도로 다이내믹 메쉬 ➡️ 지형 ➡️ 원본 순)
			auto AdjustZ = [&](FVector& Pt)
			{
				float TargetZ = Pt.Z;
				bool bSnapped = false;

				// 1) 월드 정밀 물리 레이캐스트 (컴플렉스 콜리전 레이트레이싱)
				UWorld* World = GetWorld();
				if (World)
				{
					// 원본 Z 좌표보다 위아래 50m 범위로 수직 광선을 투사
					FVector Start = FVector(Pt.X, Pt.Y, Pt.Z + 5000.f);
					FVector End = FVector(Pt.X, Pt.Y, Pt.Z - 5000.f);

					FHitResult HitResult;
					FCollisionQueryParams CollisionParams;
					CollisionParams.bTraceComplex = true; // 단순 콜리전 박스가 아닌 실제 삼각 렌더 폴리곤 표면 타겟팅
					CollisionParams.AddIgnoredActor(this);
					if (OutputLaneDynamicMeshActor)
					{
						CollisionParams.AddIgnoredActor(OutputLaneDynamicMeshActor);
					}

					// 지정된 커스텀 채널(Road - ECC_GameTraceChannel1)로 레이캐스트하여 가장 위에 부딪힌 도로 표면 획득
					if (World->LineTraceSingleByChannel(HitResult, Start, End, ECC_GameTraceChannel1, CollisionParams))
					{
						AActor* HitActor = HitResult.GetActor();
						bool bIsTargetRoad = false;

						if (HitActor && HitActor != this)
						{
							bool bIsIgnore = false;
							if (OutputLaneDynamicMeshActor && HitActor == OutputLaneDynamicMeshActor) bIsIgnore = true;
							if (VisualizerActor && HitActor == VisualizerActor) bIsIgnore = true;
							if (HitActor->IsA(ALandscapeProxy::StaticClass())) bIsIgnore = true;

							if (!bIsIgnore)
							{
								bIsTargetRoad = true;
							}
						}

						if (bIsTargetRoad)
						{
							TargetZ = HitResult.ImpactPoint.Z;
							bSnapped = true;
						}
					}
				}

				// 2) 레이캐스트 실패 시 로컬 도로 메쉬 수학 보간 시도
				if (!bSnapped && RoadDynMesh)
				{
					float RoadZ = GetRoadMeshZ(RoadDynMesh, FVector2D(Pt.X, Pt.Y), -99999.0f, RoadTransform);
					if (RoadZ > -99000.0f)
					{
						TargetZ = RoadZ;
						bSnapped = true;
					}
				}

				// 3) 지형(Landscape) 스냅 Fallback
				if (!bSnapped && bSnapToLandscape)
				{
					float LandZ = GetLandscapeZ(Pt);
					TargetZ = LandZ;
					bSnapped = true;
				}

				Pt.Z = TargetZ + LaneMarkZOffset;
			};

			AdjustZ(PtLeft);
			AdjustZ(PtRight);

			int32 VLeft = NativeMesh.AppendVertex(PtLeft);
			int32 VRight = NativeMesh.AppendVertex(PtRight);

			// 만약 이전 구간이 실선이었고 현재도 실선이면 사각형(삼각형 2개) 연결
			if (bLastWasSolid && LastVIndexLeft != -1 && LastVIndexRight != -1)
			{
				int32 T1 = NativeMesh.AppendTriangle(LastVIndexLeft, VLeft, VRight);
				int32 T2 = NativeMesh.AppendTriangle(LastVIndexLeft, VRight, LastVIndexRight);

				if (MaterialIDs)
				{
					if (T1 >= 0) MaterialIDs->SetValue(T1, MaterialID);
					if (T2 >= 0) MaterialIDs->SetValue(T2, MaterialID);
				}
			}

			bLastWasSolid = true;
			LastVIndexLeft = VLeft;
			LastVIndexRight = VRight;
		}
	};

	for (FHDMapB2LaneRow* Row : TargetRows)
	{
		if (!Row || Row->ID.Equals(TEXT("ORIGIN")) || Row->Points.Num() < 2) continue;

		// 1. 월드 트랜스폼으로 변환
		TArray<FVector> WorldPoints;
		WorldPoints.Reserve(Row->Points.Num());
		for (const FVector& Pt : Row->Points)
		{
			WorldPoints.Add(VisTransform.TransformPosition(Pt));
		}

		// 유도선 판정: Kind가 525이거나, Kind가 515(황색선군)이면서 LaneType이 112(황색점선)인 경우
		bool bIsGuideLine = Row->Kind.Equals(TEXT("525")) || (Row->Kind.Equals(TEXT("515")) && Row->LaneType.Equals(TEXT("112")));

		// 2. 곡선 부드럽게 만들기 위해 리샘플링
		float SampleDist = LaneSampleDistance;
		if (bIsGuideLine) // 유도선
		{
			SampleDist = 20.f; // 1m 점선 조각을 정상적으로 복원하기 위해 20cm 간격으로 매우 조밀하게 샘플링
		}

		TArray<FVector> ResampledPoints;
		ResamplePoints(WorldPoints, SampleDist, ResampledPoints);

		if (ResampledPoints.Num() < 2) continue;

		// 3. LaneType 분류
		FString TypeStr = Row->LaneType;
		if (TypeStr.IsEmpty()) TypeStr = TEXT("211"); // 기본값 백색 실선

		TCHAR ColorChar = TypeStr.Len() > 0 ? TypeStr[0] : '2';
		TCHAR PatternChar = TypeStr.Len() > 2 ? TypeStr[2] : '1';

		// 유도선인 경우, 실선 코드로 들어왔더라도 강제로 단선 점선('2')으로 취급합니다.
		if (bIsGuideLine)
		{
			PatternChar = '2';
		}

		// 머티리얼 인덱스 (0: 백색, 1: 황색, 2: 청색)
		int32 MatID = 0;
		if (ColorChar == '1') MatID = 1;      // 황색
		else if (ColorChar == '3') MatID = 2; // 청색

		// 점선 간격 결정 (기본값 또는 유도선 스펙)
		float SolidLen = LaneDashedSolidLength;
		float SpaceLen = LaneDashedSpaceLength;

		if (bIsGuideLine) // 유도선
		{
			SolidLen = 100.f; // 1m
			SpaceLen = 100.f; // 1m
		}
		else
		{
			// 그 외 일반 점선 (3m 생성, 5m 공백)
			SolidLen = 300.f; // 3m
			SpaceLen = 500.f; // 5m
		}

		// 패턴에 맞게 빌드
		// '1': 실선, '2': 점선, '3': 복실선, '4': 복점선, '5': 좌점우실(좌점흔선), '6': 우점좌실(우점흔선)
		if (PatternChar == '1') // 단실선
		{
			GenerateLaneStrip(ResampledPoints, -LaneMarkWidth / 2.0f, LaneMarkWidth / 2.0f, false, SolidLen, SpaceLen, MatID);
		}
		else if (PatternChar == '2') // 단점선
		{
			GenerateLaneStrip(ResampledPoints, -LaneMarkWidth / 2.0f, LaneMarkWidth / 2.0f, true, SolidLen, SpaceLen, MatID);
		}
		else if (PatternChar == '3') // 복실선
		{
			GenerateLaneStrip(ResampledPoints, -LaneMarkGap / 2.0f - LaneMarkWidth, -LaneMarkGap / 2.0f, false, SolidLen, SpaceLen, MatID);
			GenerateLaneStrip(ResampledPoints, LaneMarkGap / 2.0f, LaneMarkGap / 2.0f + LaneMarkWidth, false, SolidLen, SpaceLen, MatID);
		}
		else if (PatternChar == '4') // 복점선
		{
			GenerateLaneStrip(ResampledPoints, -LaneMarkGap / 2.0f - LaneMarkWidth, -LaneMarkGap / 2.0f, true, SolidLen, SpaceLen, MatID);
			GenerateLaneStrip(ResampledPoints, LaneMarkGap / 2.0f, LaneMarkGap / 2.0f + LaneMarkWidth, true, SolidLen, SpaceLen, MatID);
		}
		else if (PatternChar == '5') // 좌점우실
		{
			GenerateLaneStrip(ResampledPoints, -LaneMarkGap / 2.0f - LaneMarkWidth, -LaneMarkGap / 2.0f, true, SolidLen, SpaceLen, MatID);
			GenerateLaneStrip(ResampledPoints, LaneMarkGap / 2.0f, LaneMarkGap / 2.0f + LaneMarkWidth, false, SolidLen, SpaceLen, MatID);
		}
		else if (PatternChar == '6') // 우점좌실
		{
			GenerateLaneStrip(ResampledPoints, -LaneMarkGap / 2.0f - LaneMarkWidth, -LaneMarkGap / 2.0f, false, SolidLen, SpaceLen, MatID);
			GenerateLaneStrip(ResampledPoints, LaneMarkGap / 2.0f, LaneMarkGap / 2.0f + LaneMarkWidth, true, SolidLen, SpaceLen, MatID);
		}
		else
		{
			GenerateLaneStrip(ResampledPoints, -LaneMarkWidth / 2.0f, LaneMarkWidth / 2.0f, false, SolidLen, SpaceLen, MatID);
		}
	}

	int32 FinalVertexCount = NativeMesh.VertexCount();
	int32 FinalTriangleCount = NativeMesh.TriangleCount();

	// 최종 메쉬 적용 및 업데이트 알림
	DynMesh->SetMesh(MoveTemp(NativeMesh));
	DynMeshComp->NotifyMeshUpdated();

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Lane Mesh Generation Completed. Vertices: %d, Triangles: %d"),
		FinalVertexCount, FinalTriangleCount);
}

void AHDMapMeshGenerator::SaveLaneToStaticMeshAsset()
{
	if (!OutputLaneDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: OutputLaneDynamicMeshActor is not specified."));
		return;
	}

	UDynamicMeshComponent* DynMeshComp = OutputLaneDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp) return;

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh || DynMesh->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: DynamicMesh is empty. Please generate lane mesh first."));
		return;
	}

	if (SaveLaneAssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] SaveLaneAssetPath is empty. Please define a path like /Game/HDMap/SM_NamsanLane."));
		return;
	}

	FString PackagePath = SaveLaneAssetPath;
	FString AssetName;
	int32 LastSlashIndex;
	if (PackagePath.FindLastChar('/', LastSlashIndex))
	{
		AssetName = PackagePath.RightChop(LastSlashIndex + 1);
	}
	else
	{
		AssetName = PackagePath;
		PackagePath = TEXT("/Game/") + AssetName;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create package at %s"), *PackagePath);
		return;
	}
	Package->FullyLoad();

	UStaticMesh* TargetStaticMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *PackagePath));
	if (!TargetStaticMesh)
	{
		TargetStaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}

	if (!TargetStaticMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create or load StaticMesh object."));
		return;
	}

	// DynamicMesh에 바인딩된 머티리얼 목록을 StaticMesh 에셋에 할당
	TargetStaticMesh->GetStaticMaterials().Empty();
	TargetStaticMesh->GetStaticMaterials().Add(FStaticMaterial(WhiteMaterial, TEXT("WhiteMaterial")));
	TargetStaticMesh->GetStaticMaterials().Add(FStaticMaterial(YellowMaterial, TEXT("YellowMaterial")));
	TargetStaticMesh->GetStaticMaterials().Add(FStaticMaterial(BlueMaterial, TEXT("BlueMaterial")));

	FGeometryScriptCopyMeshToAssetOptions CopyOptions;
	CopyOptions.bEnableRecomputeNormals = true;
	CopyOptions.bEnableRecomputeTangents = true;
	CopyOptions.bEnableRemoveDegenerates = true;
	FGeometryScriptMeshWriteLOD TargetLOD;
	TargetLOD.LODIndex = 0;

	EGeometryScriptOutcomePins Outcome;
	
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		DynMesh,
		TargetStaticMesh,
		CopyOptions,
		TargetLOD,
		Outcome
	);

	if (Outcome == EGeometryScriptOutcomePins::Success)
	{
		FAssetRegistryModule::AssetCreated(TargetStaticMesh);
		TargetStaticMesh->MarkPackageDirty();
		
		UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied and saved StaticMesh Asset at: %s"), *PackagePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to copy DynamicMesh to StaticMesh asset."));
	}
}

void AHDMapMeshGenerator::GenerateSpeedBumpMesh()
{
	if (!VisualizerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] VisualizerActor is not specified."));
		return;
	}

	if (!OutputSpeedBumpDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] OutputSpeedBumpDynamicMeshActor is not specified."));
		return;
	}

	OutputSpeedBumpDynamicMeshActor->SetActorTransform(FTransform::Identity);
	FTransform VisTransform = VisualizerActor->GetActorTransform();

	UDataTable* SpeedBumpTable = VisualizerActor->DT_C4_SpeedBump;
	if (!SpeedBumpTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] DT_C4_SpeedBump is not mapped."));
		return;
	}

	TArray<FHDMapC4SpeedBumpRow*> TargetRows;
	SpeedBumpTable->GetAllRows<FHDMapC4SpeedBumpRow>(TEXT("HDMapMeshGen_SpeedBumpSearch"), TargetRows);

	UDynamicMeshComponent* DynMeshComp = OutputSpeedBumpDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp) return;

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh) return;

	DynMesh->Reset();

	// 도로 스냅을 위한 타겟 도로 메쉬 획득 (차선 생성 시와 100% 동일하게 일치시킵니다)
	UDynamicMesh* RoadDynMesh = nullptr;
	FTransform RoadTransform = FTransform::Identity;
	UDynamicMesh* TempRoadDynMesh = nullptr; // GC 방지용 임시 라이프사이클 참조

	if (TargetRoadStaticMeshActor)
	{
		UStaticMeshComponent* SMComp = TargetRoadStaticMeshActor->GetStaticMeshComponent();
		if (SMComp && SMComp->GetStaticMesh())
		{
			UStaticMesh* RoadSM = SMComp->GetStaticMesh();
			TempRoadDynMesh = NewObject<UDynamicMesh>();
			
			FGeometryScriptCopyMeshFromAssetOptions CopyOptions;
			FGeometryScriptMeshReadLOD TargetLOD;
			TargetLOD.LODIndex = 0;
			EGeometryScriptOutcomePins Outcome;

			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				RoadSM,
				TempRoadDynMesh,
				CopyOptions,
				TargetLOD,
				Outcome
			);

			if (Outcome == EGeometryScriptOutcomePins::Success)
			{
				RoadDynMesh = TempRoadDynMesh;
				RoadTransform = TargetRoadStaticMeshActor->GetActorTransform();
				UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied static mesh '%s' for speed bump snapping."), *RoadSM->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Failed to copy static mesh to temp dynamic mesh for speed bump snapping."));
			}
		}
	}
	else if (OutputDynamicMeshActor)
	{
		UDynamicMeshComponent* RoadDynMeshComp = OutputDynamicMeshActor->GetDynamicMeshComponent();
		if (RoadDynMeshComp)
		{
			RoadDynMesh = RoadDynMeshComp->GetDynamicMesh();
			RoadTransform = OutputDynamicMeshActor->GetActorTransform();
		}
	}

	// 다각형 경계선을 특정 간격(예: 30cm)으로 조밀하게 세분화(리샘플링)하는 헬퍼 람다
	auto ResamplePolygonPoints = [](const TArray<FVector>& OriginalPoints, float MaxStep, TArray<FVector>& OutResampled)
	{
		if (OriginalPoints.Num() < 2) return;
		
		for (int32 i = 0; i < OriginalPoints.Num(); ++i)
		{
			FVector P0 = OriginalPoints[i];
			FVector P1 = OriginalPoints[(i + 1) % OriginalPoints.Num()]; // 닫힌 루프이므로 순환
			
			OutResampled.Add(P0);
			float Dist = FVector::Dist(P0, P1);
			if (Dist > MaxStep)
			{
				int32 Steps = FMath::CeilToInt(Dist / MaxStep);
				for (int32 s = 1; s < Steps; ++s)
				{
					float T = (float)s / (float)Steps;
					OutResampled.Add(FMath::Lerp(P0, P1, T));
				}
			}
		}
	};

	UE::Geometry::FDynamicMesh3 NativeMesh;

	// Z축 정렬용 헬퍼 람다 (방지턱 원본 고도 유실을 대비하여 월드 상하방 500m 스캔)
	auto AdjustZ = [&](const FVector& Pt) -> float
	{
		float TargetZ = Pt.Z;
		bool bSnapped = false;

		UWorld* World = GetWorld();
		if (World)
		{
			// 원본 Z 좌표보다 위아래 50m 범위로 수직 광선을 투사 (차선 생성과 완전히 동일)
			FVector Start = FVector(Pt.X, Pt.Y, Pt.Z + 5000.f);
			FVector End = FVector(Pt.X, Pt.Y, Pt.Z - 5000.f);

			FHitResult HitResult;
			FCollisionQueryParams CollisionParams;
			CollisionParams.bTraceComplex = true;
			CollisionParams.AddIgnoredActor(this);
			if (OutputSpeedBumpDynamicMeshActor)
			{
				CollisionParams.AddIgnoredActor(OutputSpeedBumpDynamicMeshActor);
			}

			if (World->LineTraceSingleByChannel(HitResult, Start, End, ECC_GameTraceChannel1, CollisionParams))
			{
				AActor* HitActor = HitResult.GetActor();
				bool bIsTargetRoad = false;

				if (HitActor && HitActor != this)
				{
					bool bIsIgnore = false;
					if (OutputSpeedBumpDynamicMeshActor && HitActor == OutputSpeedBumpDynamicMeshActor) bIsIgnore = true;
					if (VisualizerActor && HitActor == VisualizerActor) bIsIgnore = true;
					if (HitActor->IsA(ALandscapeProxy::StaticClass())) bIsIgnore = true;

					if (!bIsIgnore)
					{
						bIsTargetRoad = true;
					}
				}

				if (bIsTargetRoad)
				{
					TargetZ = HitResult.ImpactPoint.Z;
					bSnapped = true;
				}
			}
		}

		if (!bSnapped && RoadDynMesh)
		{
			float RoadZ = GetRoadMeshZ(RoadDynMesh, FVector2D(Pt.X, Pt.Y), -99999.0f, RoadTransform);
			if (RoadZ > -99000.0f)
			{
				TargetZ = RoadZ;
				bSnapped = true;
			}
		}

		if (!bSnapped && bSnapToLandscape)
		{
			float LandZ = GetLandscapeZ(Pt);
			TargetZ = LandZ;
			bSnapped = true;
		}

		return TargetZ;
	};

	int32 SpeedBumpCount = 0;

	for (FHDMapC4SpeedBumpRow* Row : TargetRows)
	{
		if (!Row || Row->ID.Equals(TEXT("ORIGIN")) || Row->Points.Num() < 3) continue;

		TArray<FVector> WorldPoints;
		WorldPoints.Reserve(Row->Points.Num());
		for (const FVector& Pt : Row->Points)
		{
			WorldPoints.Add(VisTransform.TransformPosition(Pt));
		}

		// Type 분석: 기본값은 높이 있는 방지턱("1"), 그 외 "2", "3"은 납작한 방지턱
		FString SpeedBumpType = Row->Type;
		if (SpeedBumpType.IsEmpty()) SpeedBumpType = TEXT("1");

		TArray<FVector> DelaunayPoints;
		TArray<FIntVector> Triangles;

		if (SpeedBumpType.Equals(TEXT("1"))) // Type 1: 둥근 3D 아치형 방지턱
		{
			// 다각형 영역의 2D 바운딩 박스
			FBox2D PolyBox(EForceInit::ForceInit);
			for (const FVector& Pt : WorldPoints)
			{
				PolyBox += FVector2D(Pt.X, Pt.Y);
			}

			// 1) 경계 정점을 SpeedBumpGridSpacing 간격으로 조밀하게 세분화하여 추가 (경계부 디테일 정합 극대화)
			ResamplePolygonPoints(WorldPoints, SpeedBumpGridSpacing, DelaunayPoints);

			// 2) 내부 정점 그리드 분할 (Grid Refinement)
			float LocalSpacing = SpeedBumpGridSpacing; // 지정된 간격으로 조밀하게 배치
			for (float GridX = PolyBox.Min.X + LocalSpacing; GridX < PolyBox.Max.X; GridX += LocalSpacing)
			{
				for (float GridY = PolyBox.Min.Y + LocalSpacing; GridY < PolyBox.Max.Y; GridY += LocalSpacing)
				{
					FVector2D TestPt(GridX, GridY);
					if (IsPointInPolygon2D(TestPt, WorldPoints))
					{
						// 내부 그리드 정점도 원본 기준 고도인 WorldPoints[0].Z를 부여하여 차선과 동일한 ±50m 레이트레이싱 스캔 범위 내에 정확히 물리도록 합니다.
						DelaunayPoints.Add(FVector(GridX, GridY, WorldPoints[0].Z));
					}
				}
			}

			if (DelaunayPoints.Num() < 3) continue;

			// 3) 각 정점의 고도를 스냅 및 경사보간 처리
			float MaxDistToBoundary = 0.0f;
			TArray<float> Distances;
			Distances.Reserve(DelaunayPoints.Num());
			for (const FVector& Pt : DelaunayPoints)
			{
				float Dist = ComputeDistToPolygon2D(FVector2D(Pt.X, Pt.Y), WorldPoints);
				Distances.Add(Dist);
				if (Dist > MaxDistToBoundary)
				{
					MaxDistToBoundary = Dist;
				}
			}

			float Denominator = MaxDistToBoundary > 1.0f ? MaxDistToBoundary : 1.0f;

			for (int32 Index = 0; Index < DelaunayPoints.Num(); ++Index)
			{
				FVector& Pt = DelaunayPoints[Index];
				float SnappedZ = AdjustZ(Pt);
				float DistToBoundary = Distances[Index];

				// 최대 거리를 기준으로 0.0 ~ 1.0 비율 보간
				float HeightRatio = FMath::Clamp(DistToBoundary / Denominator, 0.0f, 1.0f);
				float ArchRatio = FMath::Sin(HeightRatio * HALF_PI); // 아치형 사인 곡선 보간

				// 높이 부여 및 미세 뚫림 방지 오프셋 적용
				Pt.Z = SnappedZ + (SpeedBumpHeight * ArchRatio) + SpeedBumpZOffset;
			}

			// 4) 삼각분할 수행
			if (RunDelaunayTriangulation(DelaunayPoints, Triangles))
			{
				int32 VOffset = NativeMesh.VertexCount();
				TArray<int32> GlobalIndices;
				GlobalIndices.Reserve(DelaunayPoints.Num());

				for (const FVector& Pt : DelaunayPoints)
				{
					GlobalIndices.Add(NativeMesh.AppendVertex(Pt));
				}

				for (const FIntVector& Tri : Triangles)
				{
					FVector V0 = DelaunayPoints[Tri.X];
					FVector V1 = DelaunayPoints[Tri.Y];
					FVector V2 = DelaunayPoints[Tri.Z];
					FVector Centroid = (V0 + V1 + V2) / 3.0f;

					if (IsPointInPolygon2D(FVector2D(Centroid.X, Centroid.Y), WorldPoints))
					{
						FVector E0 = V1 - V0;
						FVector E1 = V2 - V0;
						FVector CrossVal = FVector::CrossProduct(E0, E1);

						int32 FinalY = Tri.Y;
						int32 FinalZ = Tri.Z;
						if (CrossVal.Z > 0.f)
						{
							FinalY = Tri.Z;
							FinalZ = Tri.Y;
						}

						int32 T_V0 = GlobalIndices[Tri.X];
						int32 T_V1 = GlobalIndices[FinalY];
						int32 T_V2 = GlobalIndices[FinalZ];

						if (T_V0 != T_V1 && T_V1 != T_V2 && T_V2 != T_V0)
						{
							NativeMesh.AppendTriangle(T_V0, T_V1, T_V2);
						}
					}
				}
				SpeedBumpCount++;
			}
		}
		else // Type 2, 3: 납작한 평면형 방지턱
		{
			// 다각형 영역의 2D 바운딩 박스
			FBox2D PolyBox(EForceInit::ForceInit);
			for (const FVector& Pt : WorldPoints)
			{
				PolyBox += FVector2D(Pt.X, Pt.Y);
			}

			// 1) 경계 정점을 SpeedBumpGridSpacing 간격으로 조밀하게 세분화하여 추가 (경계부 디테일 정합 극대화)
			ResamplePolygonPoints(WorldPoints, SpeedBumpGridSpacing, DelaunayPoints);

			// 2) 내부 정점 그리드 분할 (Grid Refinement)
			float LocalSpacing = SpeedBumpGridSpacing; // 지정된 간격으로 조밀하게 배치
			for (float GridX = PolyBox.Min.X + LocalSpacing; GridX < PolyBox.Max.X; GridX += LocalSpacing)
			{
				for (float GridY = PolyBox.Min.Y + LocalSpacing; GridY < PolyBox.Max.Y; GridY += LocalSpacing)
				{
					FVector2D TestPt(GridX, GridY);
					if (IsPointInPolygon2D(TestPt, WorldPoints))
					{
						// 내부 그리드 정점도 원본 기준 고도인 WorldPoints[0].Z를 부여하여 차선과 동일한 ±50m 레이트레이싱 스캔 범위 내에 정확히 물리도록 합니다.
						DelaunayPoints.Add(FVector(GridX, GridY, WorldPoints[0].Z));
					}
				}
			}

			if (DelaunayPoints.Num() < 3) continue;

			// 3) 각 정점의 고도를 스냅 처리
			for (FVector& Pt : DelaunayPoints)
			{
				float SnappedZ = AdjustZ(Pt);
				// 미세 뚫림 방지 및 깜빡임 방지 오프셋 적용
				Pt.Z = SnappedZ + SpeedBumpZOffset;
			}

			if (RunDelaunayTriangulation(DelaunayPoints, Triangles))
			{
				int32 VOffset = NativeMesh.VertexCount();
				TArray<int32> GlobalIndices;
				GlobalIndices.Reserve(DelaunayPoints.Num());

				for (const FVector& Pt : DelaunayPoints)
				{
					GlobalIndices.Add(NativeMesh.AppendVertex(Pt));
				}

				for (const FIntVector& Tri : Triangles)
				{
					FVector V0 = DelaunayPoints[Tri.X];
					FVector V1 = DelaunayPoints[Tri.Y];
					FVector V2 = DelaunayPoints[Tri.Z];
					FVector Centroid = (V0 + V1 + V2) / 3.0f;

					if (IsPointInPolygon2D(FVector2D(Centroid.X, Centroid.Y), WorldPoints))
					{
						FVector E0 = V1 - V0;
						FVector E1 = V2 - V0;
						FVector CrossVal = FVector::CrossProduct(E0, E1);

						int32 FinalY = Tri.Y;
						int32 FinalZ = Tri.Z;
						if (CrossVal.Z > 0.f)
						{
							FinalY = Tri.Z;
							FinalZ = Tri.Y;
						}

						int32 T_V0 = GlobalIndices[Tri.X];
						int32 T_V1 = GlobalIndices[FinalY];
						int32 T_V2 = GlobalIndices[FinalZ];

						if (T_V0 != T_V1 && T_V1 != T_V2 && T_V2 != T_V0)
						{
							NativeMesh.AppendTriangle(T_V0, T_V1, T_V2);
						}
					}
				}
				SpeedBumpCount++;
			}
		}
	}

	int32 FinalVertexCount = NativeMesh.VertexCount();
	int32 FinalTriangleCount = NativeMesh.TriangleCount();

	DynMesh->SetMesh(MoveTemp(NativeMesh));
	
	// 다이내믹 메쉬 컴포넌트에 과속 방지턱 기본 머티리얼을 할당합니다.
	DynMeshComp->SetMaterial(0, SpeedBumpMaterial);

	DynMeshComp->NotifyMeshUpdated();

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] SpeedBump Mesh Generation Completed. Generated %d bumps. Vertices: %d, Triangles: %d"),
		SpeedBumpCount, FinalVertexCount, FinalTriangleCount);
}

void AHDMapMeshGenerator::SaveSpeedBumpToStaticMeshAsset()
{
	if (!OutputSpeedBumpDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: OutputSpeedBumpDynamicMeshActor is not specified."));
		return;
	}

	UDynamicMeshComponent* DynMeshComp = OutputSpeedBumpDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp) return;

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh || DynMesh->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: DynamicMesh is empty. Please generate speed bump mesh first."));
		return;
	}

	if (SaveSpeedBumpAssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] SaveSpeedBumpAssetPath is empty. Please define a path like /Game/HDMap/SM_NamsanSpeedBump."));
		return;
	}

	FString PackagePath = SaveSpeedBumpAssetPath;
	FString AssetName;
	int32 LastSlashIndex;
	if (PackagePath.FindLastChar('/', LastSlashIndex))
	{
		AssetName = PackagePath.RightChop(LastSlashIndex + 1);
	}
	else
	{
		AssetName = PackagePath;
		PackagePath = TEXT("/Game/") + AssetName;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create package at %s"), *PackagePath);
		return;
	}
	Package->FullyLoad();

	UStaticMesh* TargetStaticMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *PackagePath));
	if (!TargetStaticMesh)
	{
		TargetStaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}

	if (!TargetStaticMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create or load StaticMesh object."));
		return;
	}

	TargetStaticMesh->GetStaticMaterials().Empty();
	TargetStaticMesh->GetStaticMaterials().Add(FStaticMaterial(SpeedBumpMaterial, TEXT("SpeedBumpMaterial")));

	FGeometryScriptCopyMeshToAssetOptions CopyOptions;
	CopyOptions.bEnableRecomputeNormals = true;
	CopyOptions.bEnableRecomputeTangents = true;
	CopyOptions.bEnableRemoveDegenerates = true;
	FGeometryScriptMeshWriteLOD TargetLOD;
	TargetLOD.LODIndex = 0;

	EGeometryScriptOutcomePins Outcome;
	
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		DynMesh,
		TargetStaticMesh,
		CopyOptions,
		TargetLOD,
		Outcome
	);

	if (Outcome == EGeometryScriptOutcomePins::Success)
	{
		FAssetRegistryModule::AssetCreated(TargetStaticMesh);
		TargetStaticMesh->MarkPackageDirty();
		
		UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied and saved StaticMesh Asset at: %s"), *PackagePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to copy DynamicMesh to StaticMesh asset."));
	}
}

void AHDMapMeshGenerator::GenerateMarkMesh()
{
	if (!VisualizerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] VisualizerActor is not specified."));
		return;
	}

	if (!OutputMarkDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] OutputMarkDynamicMeshActor is not specified."));
		return;
	}

	UDataTable* MarkTable = VisualizerActor->DT_B3_Mark;
	if (!MarkTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] DT_B3_Mark is not mapped in visualizer."));
		return;
	}

	// DynamicMeshComponent 및 DynamicMesh 준비
	UDynamicMeshComponent* DynMeshComp = OutputMarkDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] OutputMarkDynamicMeshActor does not have a DynamicMeshComponent."));
		return;
	}

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh) return;

	// 컴포넌트 머티리얼 세팅
	DynMeshComp->SetMaterial(0, MarkAtlasMaterial);
	DynMeshComp->SetMaterial(1, CrosswalkMaterial);

	// 출력 Dynamic Mesh Actor 트랜스폼 Identity
	OutputMarkDynamicMeshActor->SetActorTransform(FTransform::Identity);
	FTransform VisTransform = VisualizerActor->GetActorTransform();

	// 도로 스냅을 위한 타겟 도로 메쉬 획득
	UDynamicMesh* RoadDynMesh = nullptr;
	FTransform RoadTransform = FTransform::Identity;
	UDynamicMesh* TempRoadDynMesh = nullptr; // GC 방지용 임시 라이프사이클 참조

	if (TargetRoadStaticMeshActor)
	{
		UStaticMeshComponent* SMComp = TargetRoadStaticMeshActor->GetStaticMeshComponent();
		if (SMComp && SMComp->GetStaticMesh())
		{
			UStaticMesh* RoadSM = SMComp->GetStaticMesh();
			TempRoadDynMesh = NewObject<UDynamicMesh>();
			
			FGeometryScriptCopyMeshFromAssetOptions CopyOptions;
			FGeometryScriptMeshReadLOD TargetLOD;
			TargetLOD.LODIndex = 0;
			EGeometryScriptOutcomePins Outcome;

			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				RoadSM,
				TempRoadDynMesh,
				CopyOptions,
				TargetLOD,
				Outcome
			);

			if (Outcome == EGeometryScriptOutcomePins::Success)
			{
				RoadDynMesh = TempRoadDynMesh;
				RoadTransform = TargetRoadStaticMeshActor->GetActorTransform();
				UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied static mesh '%s' for mark snapping."), *RoadSM->GetName());
			}
		}
	}
	else if (OutputDynamicMeshActor)
	{
		UDynamicMeshComponent* RoadDynMeshComp = OutputDynamicMeshActor->GetDynamicMeshComponent();
		if (RoadDynMeshComp)
		{
			RoadDynMesh = RoadDynMeshComp->GetDynamicMesh();
			RoadTransform = OutputDynamicMeshActor->GetActorTransform();
		}
	}

	// Native Mesh 생성용
	UE::Geometry::FDynamicMesh3 NativeMesh;
	NativeMesh.EnableAttributes();
	
	if (!NativeMesh.Attributes()->GetMaterialID())
	{
		NativeMesh.Attributes()->EnableMaterialID();
	}
	auto* MaterialIDs = NativeMesh.Attributes()->GetMaterialID();

	TArray<FHDMapB3MarkRow*> TargetRows;
	MarkTable->GetAllRows<FHDMapB3MarkRow>(TEXT("HDMapMeshGen_AllB3MarkSearch"), TargetRows);

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Generating Mark Mesh for %d marks..."), TargetRows.Num());

	// 포인트 리샘플링 헬퍼
	auto ResamplePoints = [](const TArray<FVector>& OriginalPoints, float SampleDist, TArray<FVector>& OutResampled)
	{
		if (OriginalPoints.Num() < 2) return;

		OutResampled.Add(OriginalPoints[0]);
		float AccumulatedDistance = 0.f;
		float TargetDistance = SampleDist;

		for (int32 idx = 0; idx < OriginalPoints.Num() - 1; ++idx)
		{
			FVector P0 = OriginalPoints[idx];
			FVector P1 = OriginalPoints[idx + 1];
			float SegmentLen = FVector::Dist(P0, P1);

			while (AccumulatedDistance + SegmentLen >= TargetDistance)
			{
				float Ratio = (TargetDistance - AccumulatedDistance) / SegmentLen;
				FVector InterpolatedPt = FMath::Lerp(P0, P1, Ratio);
				OutResampled.Add(InterpolatedPt);
				TargetDistance += SampleDist;
			}

			AccumulatedDistance += SegmentLen;
		}

		if (FVector::Dist(OutResampled.Last(), OriginalPoints.Last()) > SampleDist * 0.1f)
		{
			OutResampled.Add(OriginalPoints.Last());
		}
	};

	// Z 고도 정합 람다
	auto AdjustZ = [&](FVector& Pt, const FString& LineID)
	{
		float TargetZ = Pt.Z;
		bool bSnapped = false;

		UWorld* World = GetWorld();
		if (World)
		{
			FVector Start = FVector(Pt.X, Pt.Y, Pt.Z + 5000.f);
			FVector End = FVector(Pt.X, Pt.Y, Pt.Z - 5000.f);

			FHitResult HitResult;
			FCollisionQueryParams CollisionParams;
			CollisionParams.bTraceComplex = true;
			CollisionParams.AddIgnoredActor(this);
			if (OutputMarkDynamicMeshActor)
			{
				CollisionParams.AddIgnoredActor(OutputMarkDynamicMeshActor);
			}

			if (World->LineTraceSingleByChannel(HitResult, Start, End, ECC_GameTraceChannel1, CollisionParams))
			{
				AActor* HitActor = HitResult.GetActor();
				bool bIsIgnore = false;
				if (OutputMarkDynamicMeshActor && HitActor == OutputMarkDynamicMeshActor) bIsIgnore = true;
				if (VisualizerActor && HitActor == VisualizerActor) bIsIgnore = true;
				if (HitActor && HitActor->IsA(ALandscapeProxy::StaticClass())) bIsIgnore = true;

				UE_LOG(LogTemp, Log, TEXT("[GenerateMarkMesh - Raycast] LineID: %s, Hit Actor: %s, Hit Z: %.2f (Ignored: %s)"),
					*LineID,
					HitActor ? *HitActor->GetName() : TEXT("NULL"),
					HitResult.ImpactPoint.Z,
					bIsIgnore ? TEXT("True") : TEXT("False"));

				if (HitActor && !bIsIgnore)
				{
					TargetZ = HitResult.ImpactPoint.Z;
					bSnapped = true;
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[GenerateMarkMesh - Raycast] LineID: %s, FAILED to hit any road actor at XY: %.1f, %.1f"), *LineID, Pt.X, Pt.Y);
			}
		}

		if (!bSnapped && RoadDynMesh)
		{
			float RoadZ = GetRoadMeshZ(RoadDynMesh, FVector2D(Pt.X, Pt.Y), -99999.0f, RoadTransform);
			if (RoadZ > -99000.0f)
			{
				TargetZ = RoadZ;
				bSnapped = true;
			}
		}

		if (!bSnapped && bSnapToLandscape)
		{
			float LandZ = GetLandscapeZ(Pt);
			TargetZ = LandZ;
			bSnapped = true;
		}

		Pt.Z = TargetZ + MarkZOffset;
	};

	// UV 범위 도우미
	struct FMarkUVRange
	{
		float UMin;
		float UMax;
		float VMin;
		float VMax;
		bool bFlipV;
		bool bRotate90;
		float TargetWidth;
		float TargetHeight;
	};

	auto GetUVRangeByKind = [this](const FString& Kind) -> FMarkUVRange
	{
		FMarkUVRange Range;
		Range.bFlipV = false;
		Range.bRotate90 = false;
		Range.TargetWidth = 150.0f; // 기본 기호 가로폭
		Range.TargetHeight = 500.0f; // 기본 기호 세로높이

		// 1. 에디터에서 지정한 커스텀 UV 맵 매핑 테이블 먼저 확인
		if (CustomMarkUVRanges.Contains(Kind))
		{
			const FHDMapMarkUVRange& CustomRange = CustomMarkUVRanges[Kind];
			Range.UMin = CustomRange.UMin;
			Range.UMax = CustomRange.UMax;
			Range.VMin = CustomRange.VMin;
			Range.VMax = CustomRange.VMax;
			Range.bFlipV = CustomRange.bFlipV;
			Range.bRotate90 = CustomRange.bRotate90;
			Range.TargetWidth = CustomRange.TargetWidth;
			Range.TargetHeight = CustomRange.TargetHeight;
			return Range;
		}

		// 2. 없을 경우 하드코딩 기본 디폴트 아틀라스 맵 적용
		// 기본값: 직진 화살표 (U: 0.0 ~ 0.1, V: 0.0 ~ 0.5)
		Range.UMin = 0.0f; Range.UMax = 0.1f; Range.VMin = 0.0f; Range.VMax = 0.5f;

		if (Kind.Equals(TEXT("5371"))) // 직진 화살표
		{
			Range.UMin = 0.0f; Range.UMax = 0.1f; Range.VMin = 0.0f; Range.VMax = 0.5f;
		}
		else if (Kind.Equals(TEXT("5372"))) // 좌회전
		{
			Range.UMin = 0.1f; Range.UMax = 0.2f; Range.VMin = 0.0f; Range.VMax = 0.5f;
		}
		else if (Kind.Equals(TEXT("5373"))) // 우회전
		{
			Range.UMin = 0.2f; Range.UMax = 0.3f; Range.VMin = 0.0f; Range.VMax = 0.5f;
		}
		else if (Kind.Equals(TEXT("5374"))) // 좌우회전
		{
			Range.UMin = 0.3f; Range.UMax = 0.4f; Range.VMin = 0.0f; Range.VMax = 0.5f;
		}
		else if (Kind.Equals(TEXT("5379"))) // 전방향
		{
			Range.UMin = 0.4f; Range.UMax = 0.5f; Range.VMin = 0.0f; Range.VMax = 0.5f;
		}
		else if (Kind.Equals(TEXT("5381"))) // 직진및좌회전
		{
			Range.UMin = 0.5f; Range.UMax = 0.6f; Range.VMin = 0.0f; Range.VMax = 0.5f;
		}
		else if (Kind.Equals(TEXT("5382"))) // 직진및우회전
		{
			Range.UMin = 0.6f; Range.UMax = 0.7f; Range.VMin = 0.0f; Range.VMax = 0.5f;
		}
		else if (Kind.Equals(TEXT("5383"))) // 직진및유턴
		{
			Range.UMin = 0.0f; Range.UMax = 0.1f; Range.VMin = 0.5f; Range.VMax = 1.0f;
			Range.TargetWidth = 200.0f; Range.TargetHeight = 600.0f;
		}
		else if (Kind.Equals(TEXT("5391"))) // 유턴
		{
			Range.UMin = 0.1f; Range.UMax = 0.2f; Range.VMin = 0.5f; Range.VMax = 1.0f;
			Range.TargetWidth = 200.0f; Range.TargetHeight = 600.0f;
		}
		else if (Kind.Equals(TEXT("5392"))) // 좌회전및유턴
		{
			Range.UMin = 0.2f; Range.UMax = 0.3f; Range.VMin = 0.5f; Range.VMax = 1.0f;
			Range.TargetWidth = 200.0f; Range.TargetHeight = 600.0f;
		}
		else if (Kind.Equals(TEXT("5431")) || Kind.Equals(TEXT("5432"))) // 차로변경 (좌/우로합류)
		{
			Range.UMin = 0.3f; Range.UMax = 0.4f; Range.VMin = 0.5f; Range.VMax = 1.0f;
		}
		else if (Kind.Equals(TEXT("544"))) // 오르막경사면 (다이아몬드)
		{
			Range.UMin = 0.4f; Range.UMax = 0.5f; Range.VMin = 0.5f; Range.VMax = 1.0f;
		}
		else if (Kind.Equals(TEXT("5321")) || Kind.Equals(TEXT("533"))) // 횡단보도 / 고원식횡단보도
		{
			Range.UMin = 0.8f; Range.UMax = 1.0f; Range.VMin = 0.0f; Range.VMax = 1.0f;
			Range.TargetWidth = 0.0f; Range.TargetHeight = 0.0f;
		}
		else if (Kind.Equals(TEXT("534"))) // 자전거횡단보도
		{
			Range.UMin = 0.8f; Range.UMax = 1.0f; Range.VMin = 0.0f; Range.VMax = 1.0f;
			Range.TargetWidth = 0.0f; Range.TargetHeight = 0.0f;
		}

		return Range;
	};

	UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = NativeMesh.Attributes()->PrimaryUV();

	for (FHDMapB3MarkRow* Row : TargetRows)
	{
		if (!Row || Row->ID.Equals(TEXT("ORIGIN")) || Row->Points.Num() < 2) continue;

		// ⚠️ [정차금지대 524] 메쉬 생성 스킵 필터링
		if (Row->Kind.Equals(TEXT("524")))
		{
			continue;
		}

		// 월드 좌표 변환
		TArray<FVector> WorldPoints;
		WorldPoints.Reserve(Row->Points.Num());
		for (const FVector& Pt : Row->Points)
		{
			WorldPoints.Add(VisTransform.TransformPosition(Pt));
		}

		// 닫힌 루프(다각형) 판정
		bool bIsClosed = WorldPoints.Num() >= 3 && (FVector::Dist2D(WorldPoints[0], WorldPoints.Last()) < WeldDistance);

		if (bIsClosed)
		{
			// 1. Heading 방향 및 Right 방향 계산
			FVector Heading = FVector::ForwardVector;
			FVector Right = FVector::RightVector;
			bool bHeadingFound = false;

			// 다각형의 중심점(Centroid) 계산
			FVector Centroid = FVector::ZeroVector;
			for (const FVector& Pt : WorldPoints)
			{
				Centroid += Pt;
			}
			if (WorldPoints.Num() > 0)
			{
				Centroid /= (float)WorldPoints.Num();
			}

			// 소속 링크(A2_LINK) 정보를 확인하여 방향 정렬 적용
			FHDMapA2LinkRow* FoundLinkRow = nullptr;
			if (VisualizerActor && VisualizerActor->DT_A2_Link)
			{
				if (!Row->LinkID.IsEmpty())
				{
					FoundLinkRow = VisualizerActor->DT_A2_Link->FindRow<FHDMapA2LinkRow>(
						FName(*Row->LinkID), TEXT("HDMapMeshGen_MarkLinkSearch")
					);
				}

				// 횡단보도(5321, 533, 534) 등 LinkID가 없거나 검색 실패 시, 주변의 가장 가까운 링크 방향 자동 정렬 (공간 탐색)
				if (!FoundLinkRow)
				{
					TArray<FHDMapA2LinkRow*> AllLinkRows;
					VisualizerActor->DT_A2_Link->GetAllRows<FHDMapA2LinkRow>(TEXT(""), AllLinkRows);

					float BestGlobalDistSq = FLT_MAX;
					FHDMapA2LinkRow* BestLinkRow = nullptr;

					for (FHDMapA2LinkRow* TempLink : AllLinkRows)
					{
						if (!TempLink || TempLink->Points.Num() < 2) continue;

						for (int32 idx = 0; idx < TempLink->Points.Num() - 1; ++idx)
						{
							FVector P0 = VisTransform.TransformPosition(TempLink->Points[idx]);
							FVector P1 = VisTransform.TransformPosition(TempLink->Points[idx + 1]);

							FVector Seg = P1 - P0;
							FVector ToCentroid = Centroid - P0;

							float SegLenSq = Seg.SizeSquared2D();
							float T = 0.0f;
							if (SegLenSq > 1e-4f)
							{
								T = FVector::DotProduct(Seg, ToCentroid) / SegLenSq;
								T = FMath::Clamp(T, 0.0f, 1.0f);
							}

							FVector ClosestPt = P0 + Seg * T;
							float DistSq = FVector::DistSquaredXY(Centroid, ClosestPt);

							if (DistSq < BestGlobalDistSq)
							{
								BestGlobalDistSq = DistSq;
								BestLinkRow = TempLink;
							}
						}
					}

					// 최대 50m(5000cm) 이내의 링크가 존재할 경우 매핑
					if (BestGlobalDistSq < 5000.f * 5000.f)
					{
						FoundLinkRow = BestLinkRow;
					}
				}

				if (FoundLinkRow && FoundLinkRow->Points.Num() >= 2)
				{
					// 다각형 중심과 가장 가까운 링크의 세그먼트 선분 찾기
					float MinDistSq = FLT_MAX;
					FVector BestDir = FVector::ForwardVector;

					for (int32 idx = 0; idx < FoundLinkRow->Points.Num() - 1; ++idx)
					{
						FVector P0 = VisTransform.TransformPosition(FoundLinkRow->Points[idx]);
						FVector P1 = VisTransform.TransformPosition(FoundLinkRow->Points[idx + 1]);

						FVector Seg = P1 - P0;
						FVector ToCentroid = Centroid - P0;

						float SegLenSq = Seg.SizeSquared2D();
						float T = 0.0f;
						if (SegLenSq > 1e-4f)
						{
							T = FVector::DotProduct(Seg, ToCentroid) / SegLenSq;
							T = FMath::Clamp(T, 0.0f, 1.0f);
						}

						FVector ClosestPt = P0 + Seg * T;
						float DistSq = FVector::DistSquaredXY(Centroid, ClosestPt);

						if (DistSq < MinDistSq)
						{
							MinDistSq = DistSq;
							BestDir = Seg.GetSafeNormal2D();
						}
					}

					if (!BestDir.IsNearlyZero())
					{
						Heading = BestDir;
						bHeadingFound = true;
					}
				}
			}

			// 링크 정보를 찾을 수 없는 경우: 다각형 외곽 에지 기반 Fallback 계산 로직
			if (!bHeadingFound && WorldPoints.Num() >= 3)
			{
				FVector Edge0 = WorldPoints[1] - WorldPoints[0];
				FVector Edge1 = WorldPoints[2] - WorldPoints[1];
				
				float Len0 = Edge0.Size2D();
				float Len1 = Edge1.Size2D();

				if (Len1 > Len0)
				{
					Heading = Edge1.GetSafeNormal2D();
				}
				else
				{
					Heading = Edge0.GetSafeNormal2D();
				}

				if (Heading.IsNearlyZero())
				{
					Heading = FVector::ForwardVector;
				}

				// 정점 배열의 시작과 끝 흐름을 참조하여 180도 뒤집힘 방지
				FVector FlowDir = (WorldPoints[WorldPoints.Num() - 2] - WorldPoints[0]).GetSafeNormal2D();
				if (FVector::DotProduct(Heading, FlowDir) < 0.f)
				{
					Heading = -Heading;
				}
			}

			Right = FVector::CrossProduct(FVector::UpVector, Heading).GetSafeNormal2D();

			// 2. 오버라이드 확인 및 투영 축(Heading, Right) 회전 적용
			FMarkUVRange Range = GetUVRangeByKind(Row->Kind);
			bool bIsCrosswalk = Row->Kind.Equals(TEXT("5321")) || Row->Kind.Equals(TEXT("533")) || Row->Kind.Equals(TEXT("534"));
			bool bFinalFlipV = Range.bFlipV;
			bool bFinalRotate90 = Range.bRotate90;
			float FinalRotationAngle = 0.0f;
			float FinalTilingX = 1.0f;
			float FinalTilingY = 1.0f;

			if (MarkFlipOverrides.Contains(Row->ID))
			{
				bFinalFlipV = MarkFlipOverrides[Row->ID].bFlipV;
				bFinalRotate90 = MarkFlipOverrides[Row->ID].bRotate90;
				FinalRotationAngle = MarkFlipOverrides[Row->ID].RotationAngle;
				FinalTilingX = MarkFlipOverrides[Row->ID].TilingX;
				FinalTilingY = MarkFlipOverrides[Row->ID].TilingY;
				UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Mark ID '%s' Overridden. FlipV: %d, Rotate90: %d, Angle: %.2f, Tiling: %.2f, %.2f"), 
					*Row->ID, bFinalFlipV ? 1 : 0, bFinalRotate90 ? 1 : 0, FinalRotationAngle, FinalTilingX, FinalTilingY);
			}

			// 임의 회전 각도가 있는 경우, 투영 방향 축 자체를 회전시켜 로컬 좌표계 정렬
			if (!FMath::IsNearlyZero(FinalRotationAngle))
			{
				Heading = Heading.RotateAngleAxis(FinalRotationAngle, FVector::UpVector);
				Right = FVector::CrossProduct(FVector::UpVector, Heading).GetSafeNormal2D();
			}

			// 3. 로컬 2D 좌표 변환 및 로컬 AABB 계산
			TArray<FVector2D> LocalPts;
			LocalPts.Reserve(WorldPoints.Num());
			FBox2D LocalBox(EForceInit::ForceInit);

			for (const FVector& Pt : WorldPoints)
			{
				FVector2D LocalPt(FVector::DotProduct(Pt, Right), FVector::DotProduct(Pt, Heading));
				LocalPts.Add(LocalPt);
				LocalBox += LocalPt;
			}

			float SizeX = LocalBox.Max.X - LocalBox.Min.X;
			float SizeY = LocalBox.Max.Y - LocalBox.Min.Y;
			if (SizeX < 0.1f) SizeX = 1.0f;
			if (SizeY < 0.1f) SizeY = 1.0f;

			// 4. 정점 추가 및 UV 계산
			TArray<int32> VertexIndices;
			TArray<int32> UVIndices;

			for (int32 i = 0; i < WorldPoints.Num(); ++i)
			{
				FVector SnapPt = WorldPoints[i];
				AdjustZ(SnapPt, Row->ID);

				int32 VIdx = NativeMesh.AppendVertex(SnapPt);
				VertexIndices.Add(VIdx);

				// 세로(Y축)는 다각형 세로 길이에 맞춰 100% 꽉 채우기 (Stretch로 잘림 방지)
				float tY = (LocalPts[i].Y - LocalBox.Min.Y) / SizeY;

				// 가로(X축)도 다각형 가로 길이에 맞춰 100% 꽉 채우기 (Stretch로 좌우 맞춤 및 잘림 방지)
				float tX = (LocalPts[i].X - LocalBox.Min.X) / SizeX;

				float UStart = Range.UMin;
				float UEnd = Range.UMax;
				float VStart = Range.VMin;
				float VEnd = Range.VMax;

				// 반전 여부에 따라 tX, tY 투영축 반전 처리
				float tX_Final = tX;
				float tY_Final = bFinalFlipV ? tY : (1.0f - tY);

				// 90도 회전 처리 (중심 0.5 기준 시계방향 90도 회전)
				if (bFinalRotate90)
				{
					float Temp = tX_Final;
					tX_Final = tY_Final;
					tY_Final = 1.0f - Temp;
				}

				float U = 0.0f;
				float V = 0.0f;

				if (bIsCrosswalk)
				{
					// 횡단보도는 전용 Wrap 머티리얼을 사용하므로, Frac/Clamp 없이 타일링 수치를 단순 곱해 전달 (GPU Wrap 적용)
					U = tX_Final * FinalTilingX;
					V = tY_Final * FinalTilingY;
				}
				else
				{
					// 일반 노면 기호는 아틀라스 서브 영역 내에서만 안전하게 Frac 및 Clamp 처리
					if (FinalTilingX != 1.0f)
					{
						tX_Final = FMath::Frac(tX_Final * FinalTilingX);
					}
					if (FinalTilingY != 1.0f)
					{
						tY_Final = FMath::Frac(tY_Final * FinalTilingY);
					}

					float tX_Clamped = FMath::Clamp(tX_Final, 0.0f, 1.0f);
					float tY_Clamped = FMath::Clamp(tY_Final, 0.0f, 1.0f);

					U = UStart + tX_Clamped * (UEnd - UStart);
					V = VStart + tY_Clamped * (VEnd - VStart);
				}

				int32 UVIdx = UVOverlay->AppendElement(FVector2f(U, V));
				UVIndices.Add(UVIdx);
			}

			// 4. Delaunay 삼각분할
			TArray<FIntVector> Triangles2D;
			// 닫힌 루프이므로 중복된 마지막 점은 제외하고 분할용 정점 배열 구성
			TArray<FVector> PolyVerts = WorldPoints;
			if (PolyVerts.Num() > 1)
			{
				PolyVerts.RemoveAt(PolyVerts.Num() - 1);
			}

			if (RunDelaunayTriangulation(PolyVerts, Triangles2D))
			{
				for (const FIntVector& Tri : Triangles2D)
				{
					FVector V0 = PolyVerts[Tri.X];
					FVector V1 = PolyVerts[Tri.Y];
					FVector V2 = PolyVerts[Tri.Z];

					FVector TriCentroid = (V0 + V1 + V2) / 3.0f;
					FVector2D Centroid2D(TriCentroid.X, TriCentroid.Y);

					if (IsPointInPolygon2D(Centroid2D, PolyVerts))
					{
						FVector E0 = V1 - V0;
						FVector E1 = V2 - V0;
						FVector CrossVal = FVector::CrossProduct(E0, E1);

						int32 FinalY = Tri.Y;
						int32 FinalZ = Tri.Z;
						if (CrossVal.Z > 0.f)
						{
							FinalY = Tri.Z;
							FinalZ = Tri.Y;
						}

						int32 G_V0 = VertexIndices[Tri.X];
						int32 G_V1 = VertexIndices[FinalY];
						int32 G_V2 = VertexIndices[FinalZ];

						if (G_V0 != G_V1 && G_V1 != G_V2 && G_V2 != G_V0)
						{
							int32 NewTri = NativeMesh.AppendTriangle(G_V0, G_V1, G_V2);
							if (NewTri >= 0)
							{
								if (MaterialIDs)
								{
									MaterialIDs->SetValue(NewTri, bIsCrosswalk ? 1 : 0);
								}
								int32 G_UV0 = UVIndices[Tri.X];
								int32 G_UV1 = UVIndices[FinalY];
								int32 G_UV2 = UVIndices[FinalZ];
								UVOverlay->SetTriangle(NewTri, FIntVector(G_UV0, G_UV1, G_UV2));
							}
						}
					}
				}
			}
		}
	}

	// DynamicMesh에 최종 메쉬 저장
	DynMeshComp->GetDynamicMesh()->SetMesh(NativeMesh);

	int32 FinalVertexCount = NativeMesh.VertexCount();
	int32 FinalTriangleCount = NativeMesh.TriangleCount();

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Mark Mesh Generation Completed. Vertices: %d, Triangles: %d"),
		FinalVertexCount, FinalTriangleCount);
}

void AHDMapMeshGenerator::SaveMarkToStaticMeshAsset()
{
	if (!OutputMarkDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: OutputMarkDynamicMeshActor is not specified."));
		return;
	}

	UDynamicMeshComponent* DynMeshComp = OutputMarkDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp) return;

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh || DynMesh->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: DynamicMesh is empty. Please generate mark mesh first."));
		return;
	}

	if (SaveMarkAssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] SaveMarkAssetPath is empty. Please define a path like /Game/HDMap/SM_NamsanMark."));
		return;
	}

	FString PackagePath = SaveMarkAssetPath;
	FString AssetName;
	int32 LastSlashIndex;
	if (PackagePath.FindLastChar('/', LastSlashIndex))
	{
		AssetName = PackagePath.RightChop(LastSlashIndex + 1);
	}
	else
	{
		AssetName = PackagePath;
		PackagePath = TEXT("/Game/") + AssetName;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create package at %s"), *PackagePath);
		return;
	}
	Package->FullyLoad();

	UStaticMesh* TargetStaticMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *PackagePath));
	if (!TargetStaticMesh)
	{
		TargetStaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}

	if (!TargetStaticMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create or load StaticMesh object."));
		return;
	}

	TargetStaticMesh->GetStaticMaterials().Empty();
	TargetStaticMesh->GetStaticMaterials().Add(FStaticMaterial(MarkAtlasMaterial, TEXT("MarkAtlasMaterial")));
	TargetStaticMesh->GetStaticMaterials().Add(FStaticMaterial(CrosswalkMaterial, TEXT("CrosswalkMaterial")));

	FGeometryScriptCopyMeshToAssetOptions CopyOptions;
	CopyOptions.bEnableRecomputeNormals = true;
	CopyOptions.bEnableRecomputeTangents = true;
	CopyOptions.bEnableRemoveDegenerates = true;
	FGeometryScriptMeshWriteLOD TargetLOD;
	TargetLOD.LODIndex = 0;

	EGeometryScriptOutcomePins Outcome;
	
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		DynMesh,
		TargetStaticMesh,
		CopyOptions,
		TargetLOD,
		Outcome
	);

	if (Outcome == EGeometryScriptOutcomePins::Success)
	{
		FAssetRegistryModule::AssetCreated(TargetStaticMesh);
		TargetStaticMesh->MarkPackageDirty();
		
		UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied and saved StaticMesh Asset at: %s"), *PackagePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to copy DynamicMesh to StaticMesh asset."));
	}
}

void AHDMapMeshGenerator::CopyTargetRoadToDynamicMesh()
{
	if (!TargetRoadStaticMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] CopyTargetRoadToDynamicMesh: TargetRoadStaticMeshActor is not specified."));
		return;
	}

	if (!OutputDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] CopyTargetRoadToDynamicMesh: OutputDynamicMeshActor is not specified."));
		return;
	}

	UStaticMeshComponent* SMComp = TargetRoadStaticMeshActor->GetStaticMeshComponent();
	if (!SMComp || !SMComp->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] CopyTargetRoadToDynamicMesh: StaticMeshComponent or StaticMesh is invalid."));
		return;
	}

	UDynamicMeshComponent* DynMeshComp = OutputDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] CopyTargetRoadToDynamicMesh: OutputDynamicMeshActor does not have a DynamicMeshComponent."));
		return;
	}

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh) return;

	DynMesh->Reset();

	UStaticMesh* RoadSM = SMComp->GetStaticMesh();
	FGeometryScriptCopyMeshFromAssetOptions CopyOptions;
	FGeometryScriptMeshReadLOD TargetLOD;
	TargetLOD.LODIndex = 0;
	EGeometryScriptOutcomePins Outcome;

	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
		RoadSM,
		DynMesh,
		CopyOptions,
		TargetLOD,
		Outcome
	);

	if (Outcome == EGeometryScriptOutcomePins::Success)
	{
		// 월드 트랜스폼 동기화
		OutputDynamicMeshActor->SetActorTransform(TargetRoadStaticMeshActor->GetActorTransform());
		
		// 머티리얼 복사
		DynMeshComp->SetNumMaterials(SMComp->GetNumMaterials());
		for (int32 i = 0; i < SMComp->GetNumMaterials(); ++i)
		{
			DynMeshComp->SetMaterial(i, SMComp->GetMaterial(i));
		}

		// --- B3 Stamp (Direct Z-Snapped Append) 각인 처리 ---
		if (bEnableB3Stamping && VisualizerActor)
		{
			UDataTable* B3MarkTable = VisualizerActor->DT_B3_Mark;
			if (B3MarkTable)
			{
				TArray<FHDMapB3MarkRow*> B3Rows;
				B3MarkTable->GetAllRows<FHDMapB3MarkRow>(TEXT("HDMapMeshGen_B3Stamp"), B3Rows);
				
				UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] CopyTargetRoadToDynamicMesh: Processing %d B3 Marks for stamping..."), B3Rows.Num());

				FTransform VisTransform = VisualizerActor->GetActorTransform();
				FTransform TargetActorTransform = OutputDynamicMeshActor->GetActorTransform();

				// 기존 도로 폴리곤 참조용 임시 복사본 생성 후 원본 리셋 (나머지 폴리곤 삭제 효과)
				UDynamicMesh* TempRoadMesh = NewObject<UDynamicMesh>();
				if (TempRoadMesh)
				{
					TempRoadMesh->GetMeshPtr()->Copy(*DynMesh->GetMeshPtr());
					DynMesh->Reset();

					UE::Geometry::FDynamicMesh3* NativeMesh = DynMesh->GetMeshPtr();
					if (NativeMesh)
					{
						NativeMesh->EnableAttributes();
						if (!NativeMesh->Attributes()->GetMaterialID())
						{
							NativeMesh->Attributes()->EnableMaterialID();
						}
						auto* MaterialIDs = NativeMesh->Attributes()->GetMaterialID();

						#if WITH_EDITOR
						FScopedSlowTask SlowTask(B3Rows.Num(), FText::FromString(TEXT("B3 노면 표시 각인 중...")));
						SlowTask.MakeDialog(true); // 취소 버튼 활성화
						#endif

						int32 StampedCount = 0;
						for (FHDMapB3MarkRow* Row : B3Rows)
						{
							if (!Row || Row->ID.Equals(TEXT("ORIGIN")) || Row->Points.Num() < 3) continue;

							// ⚠️ [정차금지대 524] 스킵 필터링
							if (Row->Kind.Equals(TEXT("524"))) continue;

							#if WITH_EDITOR
							SlowTask.EnterProgressFrame(1.f, FText::Format(
								FText::FromString(TEXT("B3 노면 각인 중... {0} ({1}/{2})")),
								FText::FromString(Row->ID),
								FText::AsNumber(++StampedCount),
								FText::AsNumber(B3Rows.Num())
							));
							if (SlowTask.ShouldCancel())
							{
								UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] CopyTargetRoadToDynamicMesh: B3 stamping cancelled by user."));
								break;
							}
							#endif

							// 1. 다각형 월드 좌표 수집
							TArray<FVector> WorldPoints;
							for (const FVector& Pt : Row->Points)
							{
								WorldPoints.Add(VisTransform.TransformPosition(Pt));
							}

							// 마지막 중복 정점 제거
							if (WorldPoints.Num() > 1 && FVector::DistSquared2D(WorldPoints[0], WorldPoints.Last()) < WeldDistance * WeldDistance)
							{
								WorldPoints.RemoveAt(WorldPoints.Num() - 1);
							}

							if (WorldPoints.Num() < 3) continue;

							// 2. 도로 곡률 고도(Z) 획득 및 정점 로컬 변환
							TArray<int32> VertexIndices;
							VertexIndices.Reserve(WorldPoints.Num());
							TArray<FVector> LocalPoints;
							LocalPoints.Reserve(WorldPoints.Num());

							for (int32 idx = 0; idx < WorldPoints.Num(); ++idx)
							{
								FVector WP = WorldPoints[idx];
								
								// 복사해 둔 TempRoadMesh의 Z 높이를 해당 XY 좌표에서 획득하여 밀착시킴
								float SnapZ = GetRoadMeshZ(TempRoadMesh, FVector2D(WP.X, WP.Y), WP.Z, TargetActorTransform);
								WP.Z = SnapZ + B3StampHeight; // 미세 오프셋 반영

								FVector LocalPt = TargetActorTransform.InverseTransformPosition(WP);
								LocalPoints.Add(LocalPt);

								int32 VIdx = NativeMesh->AppendVertex(LocalPt);
								VertexIndices.Add(VIdx);
							}

							// 3. 2D 델로네 삼각분할 수행 및 삼각형 직접 주입
							TArray<FIntVector> Triangles2D;
							if (RunDelaunayTriangulation(LocalPoints, Triangles2D))
							{
								for (const FIntVector& Tri : Triangles2D)
								{
									FVector V0 = LocalPoints[Tri.X];
									FVector V1 = LocalPoints[Tri.Y];
									FVector V2 = LocalPoints[Tri.Z];

									FVector TriCentroid = (V0 + V1 + V2) / 3.0f;
									FVector2D Centroid2D(TriCentroid.X, TriCentroid.Y);

									if (IsPointInPolygon2D(Centroid2D, LocalPoints))
									{
										FVector E0 = V1 - V0;
										FVector E1 = V2 - V0;
										FVector CrossVal = FVector::CrossProduct(E0, E1);

										int32 FinalY = Tri.Y;
										int32 FinalZ = Tri.Z;
										if (CrossVal.Z > 0.f)
										{
											FinalY = Tri.Z;
											FinalZ = Tri.Y;
										}

										int32 G_V0 = VertexIndices[Tri.X];
										int32 G_V1 = VertexIndices[FinalY];
										int32 G_V2 = VertexIndices[FinalZ];

										if (G_V0 != G_V1 && G_V1 != G_V2 && G_V2 != G_V0)
										{
											int32 NewTri = NativeMesh->AppendTriangle(G_V0, G_V1, G_V2);
											if (NewTri >= 0 && MaterialIDs)
											{
												MaterialIDs->SetValue(NewTri, 1); // 노면 표시 머티리얼 ID 할당
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		DynMeshComp->NotifyMeshUpdated();
		UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied static mesh '%s' to OutputDynamicMeshActor."), *RoadSM->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] CopyTargetRoadToDynamicMesh: Failed to copy static mesh."));
	}
}

// EUC-KR 및 UTF-8 바이트를 직접 대조하여 한글 필드명 매칭 여부를 검사하는 헬퍼 함수
static bool MatchFieldName(const char* FieldNameRaw, const FString& TargetName)
{
	FString FieldName = FString(ANSI_TO_TCHAR(FieldNameRaw)).TrimStartAndEnd();
	if (FieldName.Equals(TargetName, ESearchCase::IgnoreCase)) return true;

	if (TargetName.Equals(TEXT("용도")))
	{
		if (FieldName.Equals(TEXT("yongdo"), ESearchCase::IgnoreCase) ||
			FieldName.Equals(TEXT("use"), ESearchCase::IgnoreCase) ||
			FieldName.Contains(TEXT("usage"), ESearchCase::IgnoreCase) ||
			FieldName.Equals(TEXT("KIND"), ESearchCase::IgnoreCase) ||
			FieldName.Equals(TEXT("KND"), ESearchCase::IgnoreCase))
		{
			return true;
		}

		const uint8 CP949_Yongdo[] = { 0xC5, 0xD9, 0xB5, 0xC5, 0 };
		const uint8 UTF8_Yongdo[] = { 0xEC, 0x9A, 0xA9, 0xEB, 0x8F, 0x84, 0 };

		if (FCStringAnsi::Stricmp(FieldNameRaw, (const char*)CP949_Yongdo) == 0 ||
			FCStringAnsi::Strnicmp(FieldNameRaw, (const char*)UTF8_Yongdo, 6) == 0)
		{
			return true;
		}
	}
	else if (TargetName.Equals(TEXT("층수")))
	{
		if (FieldName.Equals(TEXT("floor"), ESearchCase::IgnoreCase) ||
			FieldName.Equals(TEXT("floors"), ESearchCase::IgnoreCase) ||
			FieldName.Equals(TEXT("level"), ESearchCase::IgnoreCase) ||
			FieldName.Contains(TEXT("levels"), ESearchCase::IgnoreCase) ||
			FieldName.Equals(TEXT("FLR"), ESearchCase::IgnoreCase) ||
			FieldName.Equals(TEXT("FLR_CO"), ESearchCase::IgnoreCase))
		{
			return true;
		}

		const uint8 CP949_Cheungsu[] = { 0xC3, 0xFE, 0xC6, 0xE2, 0 };
		const uint8 UTF8_Cheungsu[] = { 0xEC, 0xB8, 0xB5, 0xEC, 0x88, 0x98, 0 };

		if (FCStringAnsi::Stricmp(FieldNameRaw, (const char*)CP949_Cheungsu) == 0 ||
			FCStringAnsi::Strnicmp(FieldNameRaw, (const char*)UTF8_Cheungsu, 6) == 0)
		{
			return true;
		}
	}

	return false;
}

static int32 SwapEndian(int32 Value)
{
	return ((Value >> 24) & 0x000000FF) |
		   ((Value >> 8)  & 0x0000FF00) |
		   ((Value << 8)  & 0x00FF0000) |
		   ((Value << 24) & 0xFF000000);
}

void AHDMapMeshGenerator::GenerateBuildingMesh()
{
	if (!VisualizerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] VisualizerActor is NULL!"));
		return;
	}

	if (!OutputBuildingDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] OutputBuildingDynamicMeshActor is NULL!"));
		return;
	}

	TArray<uint8> ShpData;
	TArray<uint8> DbfData;
	if (!FFileHelper::LoadFileToArray(ShpData, *BuildingShpFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to load SHP file: %s"), *BuildingShpFilePath);
		return;
	}
	if (!FFileHelper::LoadFileToArray(DbfData, *BuildingDbfFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to load DBF file: %s"), *BuildingDbfFilePath);
		return;
	}

	if (DbfData.Num() < 32)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Invalid DBF file size (too small)."));
		return;
	}

	int32 NumRecords = *reinterpret_cast<const int32*>(&DbfData[4]);
	int16 HeaderBytes = *reinterpret_cast<const int16*>(&DbfData[8]);
	int16 RecordBytes = *reinterpret_cast<const int16*>(&DbfData[10]);

	int32 NumFields = (HeaderBytes - 33) / 32;
	if (DbfData.Num() < HeaderBytes)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] DBF HeaderBytes is larger than file size."));
		return;
	}

	int32 UsageFieldOffset = -1;
	int32 UsageFieldLen = 0;
	int32 FloorFieldOffset = -1;
	int32 FloorFieldLen = 0;

	int32 CurrentFieldOffset = 1;

	for (int32 FieldIdx = 0; FieldIdx < NumFields; ++FieldIdx)
	{
		int32 DescOffset = 32 + FieldIdx * 32;
		if (DescOffset + 32 > DbfData.Num()) break;

		const char* FieldNameRaw = reinterpret_cast<const char*>(&DbfData[DescOffset]);
		uint8 FieldLength = DbfData[DescOffset + 16];

		if (MatchFieldName(FieldNameRaw, TEXT("용도")))
		{
			UsageFieldOffset = CurrentFieldOffset;
			UsageFieldLen = FieldLength;
		}
		else if (MatchFieldName(FieldNameRaw, TEXT("층수")))
		{
			FloorFieldOffset = CurrentFieldOffset;
			FloorFieldLen = FieldLength;
		}

		CurrentFieldOffset += FieldLength;
	}

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] DBF Parsed. Records: %d, UsageOffset: %d (Len:%d), FloorOffset: %d (Len:%d)"),
		NumRecords, UsageFieldOffset, UsageFieldLen, FloorFieldOffset, FloorFieldLen);

	struct FBuildingAttribute
	{
		FString Usage;
		int32 FloorCount = 1;
	};
	TArray<FBuildingAttribute> Attributes;
	Attributes.Reserve(NumRecords);

	for (int32 RecIdx = 0; RecIdx < NumRecords; ++RecIdx)
	{
		int32 RecOffset = HeaderBytes + RecIdx * RecordBytes;
		if (RecOffset + RecordBytes > DbfData.Num()) break;

		FBuildingAttribute Attr;
		if (DbfData[RecOffset] == 0x2A)
		{
			Attr.Usage = TEXT("DELETED");
			Attributes.Add(Attr);
			continue;
		}

		if (UsageFieldOffset >= 0 && UsageFieldLen > 0)
		{
			TArray<uint8> UsageBuffer;
			UsageBuffer.AddUninitialized(UsageFieldLen + 1);
			FMemory::Memcpy(UsageBuffer.GetData(), &DbfData[RecOffset + UsageFieldOffset], UsageFieldLen);
			UsageBuffer[UsageFieldLen] = 0;
			
			Attr.Usage = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(UsageBuffer.GetData()))).TrimStartAndEnd();
		}

		if (FloorFieldOffset >= 0 && FloorFieldLen > 0)
		{
			TArray<uint8> FloorBuffer;
			FloorBuffer.AddUninitialized(FloorFieldLen + 1);
			FMemory::Memcpy(FloorBuffer.GetData(), &DbfData[RecOffset + FloorFieldOffset], FloorFieldLen);
			FloorBuffer[FloorFieldLen] = 0;
			
			FString FloorStr = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(FloorBuffer.GetData()))).TrimStartAndEnd();
			Attr.FloorCount = FCString::Atoi(*FloorStr);
			if (Attr.FloorCount <= 0) Attr.FloorCount = 1;
		}

		Attributes.Add(Attr);
	}

	if (ShpData.Num() < 100)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Invalid SHP file size (too small)."));
		return;
	}

	int32 ShpShapeType = *reinterpret_cast<const int32*>(&ShpData[32]);
	if (ShpShapeType != 5)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] SHP ShapeType is not Polygon (5). Type: %d"), ShpShapeType);
	}

	OutputBuildingDynamicMeshActor->SetActorTransform(FTransform::Identity);
	UDynamicMeshComponent* DynMeshComp = OutputBuildingDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp) return;

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh) return;
	DynMesh->Reset();

	UE::Geometry::FDynamicMesh3 NativeMesh;
	NativeMesh.EnableAttributes();
	if (!NativeMesh.Attributes()->GetMaterialID())
	{
		NativeMesh.Attributes()->EnableMaterialID();
	}

	FTransform VisTransform = VisualizerActor->GetActorTransform();

	int32 ShpOffset = 100;
	int32 RecordCounter = 0;

	while (ShpOffset + 8 <= ShpData.Num())
	{
		int32 RecNum = SwapEndian(*reinterpret_cast<const int32*>(&ShpData[ShpOffset]));
		int32 ContentWords = SwapEndian(*reinterpret_cast<const int32*>(&ShpData[ShpOffset + 4]));
		int32 ContentBytes = ContentWords * 2;

		int32 DataOffset = ShpOffset + 8;
		ShpOffset = DataOffset + ContentBytes;

		if (ShpOffset > ShpData.Num()) break;

		int32 RecShapeType = *reinterpret_cast<const int32*>(&ShpData[DataOffset]);
		if (RecShapeType != 5) continue;

		int32 NumParts = *reinterpret_cast<const int32*>(&ShpData[DataOffset + 36]);
		int32 NumPoints = *reinterpret_cast<const int32*>(&ShpData[DataOffset + 40]);

		if (NumPoints < 3)
		{
			RecordCounter++;
			continue;
		}

		int32 PartsOffset = DataOffset + 44;
		int32 PointsOffset = PartsOffset + NumParts * 4;

		if (PointsOffset + NumPoints * 16 > ShpData.Num()) break;

		TArray<FVector> BoundaryWorldPoints;
		BoundaryWorldPoints.Reserve(NumPoints);

		const double* PointsRaw = reinterpret_cast<const double*>(&ShpData[PointsOffset]);
		for (int32 p = 0; p < NumPoints; ++p)
		{
			double RawX = PointsRaw[p * 2];
			double RawY = PointsRaw[p * 2 + 1];

			FVector LocalPt(RawX, RawY, 0.f);
			FVector WP = VisTransform.TransformPosition(LocalPt);
			BoundaryWorldPoints.Add(WP);
		}

		if (BoundaryWorldPoints.Num() > 1 && FVector::DistSquared2D(BoundaryWorldPoints[0], BoundaryWorldPoints.Last()) < WeldDistance * WeldDistance)
		{
			BoundaryWorldPoints.RemoveAt(BoundaryWorldPoints.Num() - 1);
		}

		int32 BoundaryCount = BoundaryWorldPoints.Num();
		if (BoundaryCount < 3)
		{
			RecordCounter++;
			continue;
		}

		float Height = BuildingBaseFloorHeight;
		if (Attributes.IsValidIndex(RecordCounter))
		{
			const FBuildingAttribute& Attr = Attributes[RecordCounter];
			if (!Attr.Usage.Equals(TEXT("DELETED")))
			{
				if (Attr.Usage.Contains(TEXT("주택")) || Attr.Usage.Contains(TEXT("숙박")) || Attr.Usage.Contains(TEXT("house")) || Attr.Usage.Contains(TEXT("hotel")) || Attr.Usage.Contains(TEXT("residence")))
				{
					Height = Attr.FloorCount * 350.f;
				}
				else if (Attr.Usage.Contains(TEXT("근린생활")) || Attr.Usage.Contains(TEXT("상가")) || Attr.Usage.Contains(TEXT("shop")) || Attr.Usage.Contains(TEXT("commercial")))
				{
					Height = Attr.FloorCount * 400.f;
				}
				else
				{
					Height = Attr.FloorCount * 500.f;
				}
			}
		}

		float CommonBaseZ = 0.f;
		if (bSnapToLandscape)
		{
			CommonBaseZ = GetLandscapeZ(BoundaryWorldPoints[0]);
		}
		else
		{
			CommonBaseZ = BoundaryWorldPoints[0].Z;
		}

		TArray<int32> BottomVertIDs;
		TArray<int32> TopVertIDs;
		BottomVertIDs.Reserve(BoundaryCount);
		TopVertIDs.Reserve(BoundaryCount);

		TArray<FVector> LocalFlatPoints;
		LocalFlatPoints.Reserve(BoundaryCount);

		for (int32 v = 0; v < BoundaryCount; ++v)
		{
			FVector BP = BoundaryWorldPoints[v];
			BP.Z = CommonBaseZ;

			FVector TP = BP;
			TP.Z += Height;

			BottomVertIDs.Add(NativeMesh.AppendVertex(BP));
			TopVertIDs.Add(NativeMesh.AppendVertex(TP));

			LocalFlatPoints.Add(FVector(BP.X, BP.Y, 0.f));
		}

		for (int32 i = 0; i < BoundaryCount; ++i)
		{
			int32 next_i = (i + 1) % BoundaryCount;
			int32 B0 = BottomVertIDs[i];
			int32 B1 = BottomVertIDs[next_i];
			int32 T0 = TopVertIDs[i];
			int32 T1 = TopVertIDs[next_i];

			NativeMesh.AppendTriangle(B0, B1, T1);
			NativeMesh.AppendTriangle(B0, T1, T0);
		}

		TArray<FIntVector> RoofTriangles;
		if (RunDelaunayTriangulation(LocalFlatPoints, RoofTriangles))
		{
			for (const FIntVector& Tri : RoofTriangles)
			{
				FVector V0 = LocalFlatPoints[Tri.X];
				FVector V1 = LocalFlatPoints[Tri.Y];
				FVector V2 = LocalFlatPoints[Tri.Z];

				FVector Centroid = (V0 + V1 + V2) / 3.f;
				FVector2D Centroid2D(Centroid.X, Centroid.Y);

				if (IsPointInPolygon2D(Centroid2D, LocalFlatPoints))
				{
					FVector E0 = V1 - V0;
					FVector E1 = V2 - V0;
					FVector CrossVal = FVector::CrossProduct(E0, E1);

					int32 FinalY = Tri.Y;
					int32 FinalZ = Tri.Z;
					if (CrossVal.Z > 0.f)
					{
						FinalY = Tri.Z;
						FinalZ = Tri.Y;
					}

					NativeMesh.AppendTriangle(TopVertIDs[Tri.X], TopVertIDs[FinalY], TopVertIDs[FinalZ]);
					NativeMesh.AppendTriangle(BottomVertIDs[Tri.X], BottomVertIDs[FinalZ], BottomVertIDs[FinalY]);
				}
			}
		}

		RecordCounter++;
	}

	DynMesh->SetMesh(NativeMesh);
	DynMeshComp->NotifyMeshUpdated();
	DynMeshComp->SetComplexAsSimpleCollisionEnabled(true, true);

	UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Building Mesh Generation Completed. Total Buildings Spawned: %d"), RecordCounter);
}

void AHDMapMeshGenerator::SaveBuildingToStaticMeshAsset()
{
	if (!OutputBuildingDynamicMeshActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: OutputBuildingDynamicMeshActor is not specified."));
		return;
	}

	UDynamicMeshComponent* DynMeshComp = OutputBuildingDynamicMeshActor->GetDynamicMeshComponent();
	if (!DynMeshComp) return;

	UDynamicMesh* DynMesh = DynMeshComp->GetDynamicMesh();
	if (!DynMesh || DynMesh->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] Cannot save: DynamicMesh is empty. Please generate building mesh first."));
		return;
	}

	if (SaveBuildingAssetPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AHDMapMeshGenerator] SaveBuildingAssetPath is empty. Please define a path."));
		return;
	}

	FString PackagePath = SaveBuildingAssetPath;
	FString AssetName;
	int32 LastSlashIndex;
	if (PackagePath.FindLastChar('/', LastSlashIndex))
	{
		AssetName = PackagePath.RightChop(LastSlashIndex + 1);
	}
	else
	{
		AssetName = PackagePath;
		PackagePath = TEXT("/Game/") + AssetName;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create package at %s"), *PackagePath);
		return;
	}
	Package->FullyLoad();

	UStaticMesh* TargetStaticMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *PackagePath));
	if (!TargetStaticMesh)
	{
		TargetStaticMesh = NewObject<UStaticMesh>(Package, FName(*AssetName), RF_Public | RF_Standalone);
	}

	if (!TargetStaticMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to create or load StaticMesh object."));
		return;
	}

	TargetStaticMesh->GetStaticMaterials().Empty();
	for (int32 i = 0; i < DynMeshComp->GetNumMaterials(); ++i)
	{
		TargetStaticMesh->GetStaticMaterials().Add(FStaticMaterial(DynMeshComp->GetMaterial(i), FName(*FString::Printf(TEXT("Material_%d"), i))));
	}

	FGeometryScriptCopyMeshToAssetOptions CopyOptions;
	CopyOptions.bEnableRecomputeNormals = true;
	CopyOptions.bEnableRecomputeTangents = true;
	CopyOptions.bEnableRemoveDegenerates = true;
	FGeometryScriptMeshWriteLOD TargetLOD;
	TargetLOD.LODIndex = 0;

	EGeometryScriptOutcomePins Outcome;
	
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		DynMesh,
		TargetStaticMesh,
		CopyOptions,
		TargetLOD,
		Outcome
	);

	if (Outcome == EGeometryScriptOutcomePins::Success)
	{
		FAssetRegistryModule::AssetCreated(TargetStaticMesh);
		TargetStaticMesh->MarkPackageDirty();
		
		UE_LOG(LogTemp, Log, TEXT("[AHDMapMeshGenerator] Successfully copied and saved Building StaticMesh Asset at: %s"), *PackagePath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[AHDMapMeshGenerator] Failed to copy DynamicMesh to StaticMesh asset."));
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "HDMapMeshGenerator.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "EngineUtils.h"

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
	LaneWidth = 300.f;          // 기본 차선 폭 3m (300cm)
	bUseDelaunay = true;        // 기본으로 델로네 삼각분할 생성 모드 사용
	bOnlyUseMapData = true;     // 기본으로 오직 MapData 다각형만 사용
	bSnapToLandscape = true;    // 기본으로 지형 스냅 켬
	TunnelRoadZ = -1500.f;      // 기본 지하 도로 깊이 -15m
	SampleDistance = 1000.f;    // 중심선 리샘플링 기본 간격 10m (1000cm)
	GridSpacing = 300.f;        // 내부 그리드 생성 간격 기본값 3m (300cm)
	WeldDistance = 50.f;        // 50cm 내의 점들은 동일 정점으로 병합
	MaxEdgeLength = 1500.f;     // 삼각형 한 변의 최대 길이 15m 제한
	MinAngleDegree = 3.f;       // 내각 제한 최소 3도 (매우 길쭉하고 얇은 노이즈 삼각형 제거)
	ZOffset = 2.f;              // 지형과의 깜빡임(Z-fighting) 방지를 위한 높이 보정치 2cm
	GridSize = 5000.f;          // 대규모 연산 부하 개선을 위한 공간 격자 크기 (50m)
	SaveAssetPath = TEXT("/Game/HDMap/SM_NamsanRoad");
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

				// 2) 도로 내부 그리드 정점 자동 추가 (Grid Refinement)
				if (!bIsTunnel && bSnapToLandscape && DwRow->Points.Num() >= 3)
				{
					FBox2D PolyBox(EForceInit::ForceInit);
					for (const FVector& Pt : DwRow->Points)
					{
						PolyBox += FVector2D(Pt.X, Pt.Y);
					}

					// 그리드 간격 안전 값 보장 (무한루프 방지 최소 50cm 제한)
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
								
								// 지형 높이 실시간 탐색 및 스냅
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
					TArray<int32> LocalToGlobalIndexMap;
					LocalToGlobalIndexMap.AddUninitialized(LocalVertices.Num());

					for (int32 LocalIdx = 0; LocalIdx < LocalVertices.Num(); ++LocalIdx)
					{
						LocalToGlobalIndexMap[LocalIdx] = FindOrAddVertex(LocalVertices[LocalIdx], WeldDistance, DwRow->ID);
					}

					for (const FIntVector& Tri : RawTriangles)
					{
						FVector V0 = LocalVertices[Tri.X];
						FVector V1 = LocalVertices[Tri.Y];
						FVector V2 = LocalVertices[Tri.Z];

						FVector Centroid = (V0 + V1 + V2) / 3.0f;
						FVector LocalCentroid = VisTransform.InverseTransformPosition(Centroid);
						FVector2D Centroid2D(LocalCentroid.X, LocalCentroid.Y);

						if (IsPointInPolygon2D(Centroid2D, DwRow->Points))
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

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HDMapVisualizer.h"
#include "DynamicMeshActor.h"
#include "HDMapMeshGenerator.generated.h"

class UDynamicMeshComponent;

USTRUCT(BlueprintType)
struct FHDMapMarkUVRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float UMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float UMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float VMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float VMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	bool bFlipV = false; // 상하 반전

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	bool bRotate90 = false; // 90도 회전

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float TargetWidth = 150.0f; // 노면 기호의 표준 가로 크기 (cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float TargetHeight = 500.0f; // 노면 기호의 표준 세로 크기 (cm)
};

USTRUCT(BlueprintType)
struct FHDMapMarkFlipOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	bool bFlipV = false; // 개별 상하 반전

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	bool bRotate90 = false; // 개별 90도 회전

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float RotationAngle = 0.0f; // 개별 회전 각도 (도 단위, 예: 45.0)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float TilingX = 1.0f; // 개별 가로 타일링 반복 횟수 (1.0 = 기본)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float TilingY = 1.0f; // 개별 세로 타일링 반복 횟수 (1.0 = 기본)

	FHDMapMarkFlipOverride()
		: bFlipV(false)
		, bRotate90(false)
		, RotationAngle(0.0f)
		, TilingX(1.0f)
		, TilingY(1.0f)
	{}
};

UCLASS()
class SHARDSOFSEOUL_API AHDMapMeshGenerator : public AActor
{
	GENERATED_BODY()
	
public:	
	AHDMapMeshGenerator();

protected:
	virtual void BeginPlay() override;

public:
	// 타겟 정밀도로지도 비주얼라이저 액터 지정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Target")
	AHDMapVisualizer* VisualizerActor;

	// 메쉬 생성 대상이 되는 Line ID 리스트 (비어있으면 비주얼라이저의 전체 라인을 빌드)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Selection")
	TArray<FString> SelectedLineIDs;

	// 도로 압출 생성 시 적용할 기본 도로 너비 (센티미터 단위, 기본값 600cm = 6m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "50.0"))
	float DefaultRoadWidth;

	// 도로 압출 생성 시 적용할 도로 수직 두께/높이 (센티미터 단위, 기본값 50cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "0.0"))
	float RoadHeight;

	// 차선 간 경계를 형성할 때 적용할 기본 차선 너비 (센티미터 단위, 기본값 300cm = 3m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "50.0"))
	float LaneWidth;

	// true 일 경우 델로네 삼각분할(Delaunay Triangulation) 방식을 사용하고, false 일 경우 리본 압출 방식을 사용합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Generation Mode")
	bool bUseDelaunay;

	// true 일 경우 다른 레이어를 배제하고 오직 MapData(HDMapDataTable) 다각형 데이터만 사용하여 메쉬를 생성합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Generation Mode")
	bool bOnlyUseMapData;

	// true 일 경우 다각형 정점들을 월드 지형(Landscape) 높이에 강제 밀착(Snap)시킵니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Generation Mode")
	bool bSnapToLandscape;

	// 지하 도로(터널)로 간주할 다각형 ID 목록 (지형 밀착을 하지 않고 TunnelRoadZ 높이로 일괄 배치됨)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Generation Mode")
	TArray<FString> TunnelRoadIDs;

	// 지하 도로 설정 시 일괄 배치할 지하 고도값 (센티미터 단위, 기본값 -1500 = 지하 15m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Generation Mode")
	float TunnelRoadZ;

	// 도로 중심선 포인트를 리샘플링할 간격 (센티미터 단위, 기본값 1000cm = 10m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "50.0"))
	float SampleDistance;

	// 도로 내부 그리드 정점 생성 간격 (센티미터 단위, 기본값 300cm = 3m, 낮을수록 촘촘해져 지형 밀착도가 극대화됩니다.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "50.0"))
	float GridSpacing;

	// true 일 경우 도로 다각형 내부에 GridSpacing 간격의 보조 정점을 생성하여 지형 밀착도를 높입니다.
	// false 로 끄면 외곽 경계 정점만으로 메쉬를 생성하므로 생성 속도가 빠르고 폴리곤 수가 줄어듭니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings")
	bool bEnableGridRefinement;

	// 점 병합(Point Welding) 반경 (센티미터 단위, 이 반경 내의 점들은 하나로 합쳐짐)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "1.0"))
	float WeldDistance;

	// 삼각형 변의 최대 허용 길이 (센티미터 단위, 도로 폭 이상의 거대한 불필요 삼각형 필터링용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "10.0"))
	float MaxEdgeLength;

	// 삼각형의 최소 내각 제한 (디그리 단위, 너무 좁고 긴 바늘 모양 삼각형 제거용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MinAngleDegree;

	// 에지(모서리) 하나당 고정 분할할 개수 (기본값 5, 1이면 분할 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "1"))
	int32 EdgeSubdivisionCount;

	// Z축 고도 오프셋 (지형 메쉬와의 Z-fighting 깜빡임 방지용 센티미터 단위 오프셋)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings")
	float ZOffset;

	// 인도 압출 생성 시 적용할 인도 수직 두께/높이 (센티미터 단위, 기본값 20cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Sidewalk Settings", meta = (ClampMin = "0.0"))
	float SidewalkHeight;

	// 인도 Z축 고도 오프셋 (도로와의 높이 정렬 및 깜빡임 방지용, 기본값 30cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Sidewalk Settings")
	float SidewalkZOffset;

	// 인도 내부 그리드 정점 생성 간격 (센티미터 단위, 기본값 200cm, 낮을수록 촘촘해져 지형 밀착도가 극대화됩니다.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Sidewalk Settings", meta = (ClampMin = "50.0"))
	float SidewalkGridSpacing;

	// true 일 경우 인도 다각형 내부에 SidewalkGridSpacing 간격의 보조 정점을 생성하여 지형 밀착도를 높입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Sidewalk Settings")
	bool bEnableSidewalkGridRefinement;

	// --- Lane Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings")
	UMaterialInterface* WhiteMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings")
	UMaterialInterface* YellowMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings")
	UMaterialInterface* BlueMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings", meta = (ClampMin = "1.0"))
	float LaneMarkWidth; // 단선 너비 (기본 15cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings", meta = (ClampMin = "0.0"))
	float LaneMarkGap; // 겹선 간격 (기본 10cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings")
	float LaneMarkZOffset; // 노면 스냅 오프셋 (기본 1cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings", meta = (ClampMin = "10.0"))
	float LaneSampleDistance; // 곡선 구현용 차선 리샘플링 간격 (기본 100cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings")
	float LaneDashedSolidLength; // 점선 생성 길이 (기본 300cm = 3m)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Lane Settings")
	float LaneDashedSpaceLength; // 점선 공백 길이 (기본 300cm = 3m)

	// --- Mark Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mark Settings")
	UMaterialInterface* MarkAtlasMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mark Settings")
	UMaterialInterface* CrosswalkMaterial; // 횡단보도 전용 머티리얼 (Wrap 반복 텍스처 적용용)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mark Settings")
	float MarkZOffset; // 노면 스냅 오프셋 (기본 1.5cm)



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mark Settings")
	TMap<FString, FHDMapMarkUVRange> CustomMarkUVRanges; // 에디터에서 직접 지정 가능한 기호(Kind)별 커스텀 UV 맵 매핑 테이블

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mark Settings")
	TMap<FString, FHDMapMarkFlipOverride> MarkFlipOverrides; // 특정 다각형 ID(Line ID)별 상하/좌우 반전 오버라이드 테이블

	// --- SpeedBump Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - SpeedBump Settings", meta = (ClampMin = "0.0"))
	float SpeedBumpHeight; // 과속 방지턱 기본 수직 두께/높이 (센티미터 단위, 기본값 10cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - SpeedBump Settings", meta = (ClampMin = "5.0"))
	float SpeedBumpStripeWidth; // 과속 방지턱 사선 빗금 패턴 한 줄의 수평 너비 (센티미터 단위, 기본값 30cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - SpeedBump Settings", meta = (ClampMin = "5.0"))
	float SpeedBumpGridSpacing; // 과속 방지턱 정점 분할 및 내부 그리드 간격 (센티미터 단위, 기본값 30cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - SpeedBump Settings")
	float SpeedBumpZOffset; // 도로 뚫림 방지용 과속 방지턱 Z축 미세 추가 오프셋 (센티미터 단위, 기본값 1.0cm)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - SpeedBump Settings")
	UMaterialInterface* SpeedBumpMaterial; // 과속 방지턱에 할당할 기본 머티리얼 에셋 (기존 유지)

	// --- B3 Stamp Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - B3 Stamp Settings")
	bool bEnableB3Stamping; // 도로 메쉬에 B3 다각형을 도장으로 각인할지 여부

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - B3 Stamp Settings")
	float B3StampHeight; // 도로 표면 위로 각인할 미세 높이 (cm, 기본 0.1f = 1mm 미세 오프셋)

	// 공간 분할 격자 크기 (센티미터 단위, 대규모 정점의 삼각분할 연산 속도 개선 및 왜곡 방지용 격자 크기)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "500.0"))
	float GridSize;

	// 메쉬 생성 결과를 구워낼 타겟 Dynamic Mesh Actor (월드 상의 액터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	ADynamicMeshActor* OutputDynamicMeshActor;

	// 인도 메쉬 생성 결과를 구워낼 타겟 Dynamic Mesh Actor (월드 상의 액터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	ADynamicMeshActor* OutputSidewalkDynamicMeshActor;

	// 차선 메쉬 생성 결과를 구워낼 타겟 Dynamic Mesh Actor (월드 상의 액터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	ADynamicMeshActor* OutputLaneDynamicMeshActor;

	// 과속 방지턱 메쉬 생성 결과를 구워낼 타겟 Dynamic Mesh Actor (월드 상의 액터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	ADynamicMeshActor* OutputSpeedBumpDynamicMeshActor;

	// Static Mesh로 구울 때 저장할 패키지 패스 (예: /Game/HDMap/SM_NamsanRoad)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	FString SaveAssetPath;

	// 인도 메쉬를 Static Mesh로 구울 때 저장할 패키지 패스 (예: /Game/HDMap/SM_NamsanSidewalk)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	FString SaveSidewalkAssetPath;

	// 차선 메쉬를 Static Mesh로 구울 때 저장할 패키지 패스 (예: /Game/HDMap/SM_NamsanLane)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	FString SaveLaneAssetPath;

	// 과속 방지턱 메쉬를 Static Mesh로 구울 때 저장할 패키지 패스 (예: /Game/HDMap/SM_NamsanSpeedBump)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	FString SaveSpeedBumpAssetPath;

	// 노면표시 메쉬 생성 결과를 구워낼 타겟 Dynamic Mesh Actor (월드 상의 액터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	ADynamicMeshActor* OutputMarkDynamicMeshActor;

	// 노면표시 메쉬를 Static Mesh 에셋을 저장할 패키지 패스 (예: /Game/HDMap/SM_NamsanMark)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	FString SaveMarkAssetPath;

	// 건물 메쉬 생성 결과를 구워낼 타겟 Dynamic Mesh Actor (월드 상의 액터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	ADynamicMeshActor* OutputBuildingDynamicMeshActor;

	// 건물 메쉬를 Static Mesh 에셋으로 저장할 패키지 패스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	FString SaveBuildingAssetPath;

	// 건물 외곽선 데이터 (.shp) 파일 절대 경로
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Building Settings")
	FString BuildingShpFilePath;

	// 건물 속성 데이터 (.dbf) 파일 절대 경로
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Building Settings")
	FString BuildingDbfFilePath;

	// 기본 건물 1층 높이 (cm 단위, 기본값 350.f = 3.5m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Building Settings")
	float BuildingBaseFloorHeight;

	// 높이 분석 타겟으로 삼을 월드에 배치된 도로 Static Mesh Actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Snap Target")
	class AStaticMeshActor* TargetRoadStaticMeshActor;

	// [Landscape Carve] 도로 노면 Z 기준으로 얼마나 더 아래를 깎을지 (cm, 음수 = 더 깊이)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Landscape Carve")
	float CarveZOffset;

	// [Landscape Carve] 도로 경계 바깥으로 자연스럽게 경사를 이어붙일 Feather 반경 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Landscape Carve", meta = (ClampMin = "0.0"))
	float CarveFeatherRadius;

	// 도로 노면 메쉬 생성 기능 호출 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void GenerateRoadMesh();

	// 생성된 메쉬를 Static Mesh 에셋으로 저장하는 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void SaveToStaticMeshAsset();

	// 인도 메쉬 생성 기능 호출 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void GenerateSidewalkMesh();

	// 생성된 인도 메쉬를 Static Mesh 에셋으로 저장하는 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void SaveSidewalkToStaticMeshAsset();

	// 차선 메쉬 생성 기능 호출 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void GenerateLaneMesh();

	// 생성된 차선 메쉬를 Static Mesh 에셋으로 저장하는 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void SaveLaneToStaticMeshAsset();

	// 과속 방지턱 메쉬 생성 기능 호출 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void GenerateSpeedBumpMesh();

	// 생성된 과속 방지턱 메쉬를 Static Mesh 에셋으로 저장하는 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void SaveSpeedBumpToStaticMeshAsset();

	// 노면표시 메쉬 생성 기능 호출 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void GenerateMarkMesh();

	// 생성된 노면표시 메쉬를 Static Mesh 에셋으로 저장하는 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void SaveMarkToStaticMeshAsset();

	// 건물 메쉬 생성 기능 호출 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void GenerateBuildingMesh();

	// 생성된 건물 메쉬를 Static Mesh 에셋으로 저장하는 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void SaveBuildingToStaticMeshAsset();

	// [R&D] 타겟 로드 스태틱 메쉬의 폴리곤을 출력 다이내믹 메쉬로 그대로 복사하는 기능
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void CopyTargetRoadToDynamicMesh();

	// 도로 다각형 영역에 맞춰 Landscape 높이맵을 직접 깎아내는 에디터 전용 기능
	// ⚠️ 실행 전 레벨 저장/백업 권장 (Ctrl+Z로 Undo 가능)
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void CarveLandscapeForRoads();

private:
	// 2D 델로네 삼각분할을 수행하는 핵심 헬퍼 함수
	bool RunDelaunayTriangulation(const TArray<FVector>& Vertices, TArray<FIntVector>& OutTriangles);

	// 두 2D 평면 벡터 사이의 거리 측정용 헬퍼
	float GetDistance2D(const FVector& V1, const FVector& V2) const;

	// 다각형 내부 판정용 고속 레이캐스팅 함수 (2D 투영)
	static bool IsPointInPolygon2D(const FVector2D& Point, const TArray<FVector>& PolygonPoints);

	// 레이트레이싱을 통해 월드 지형의 Z 고도값 탐색
	float GetLandscapeZ(const FVector& WorldPos);

	// 월드 XY 좌표점에서 다각형(월드 좌표 FVector 배열) 외곽 에지까지의 최소 2D 거리 반환
	static float ComputeDistToPolygon2D(const FVector2D& Point, const TArray<FVector>& PolyWorldPoints);

	// 특정 2D 좌표에서 도로 메쉬의 높이(Z)를 구하고, 없을 경우 FallbackZ를 반환
	float GetRoadMeshZ(class UDynamicMesh* RoadDynMesh, const FVector2D& Pt, float FallbackZ, const FTransform& ActorTransform);
};

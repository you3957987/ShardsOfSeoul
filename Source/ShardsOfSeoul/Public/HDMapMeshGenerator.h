// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HDMapVisualizer.h"
#include "DynamicMeshActor.h"
#include "HDMapMeshGenerator.generated.h"

class UDynamicMeshComponent;

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



	// 점 병합(Point Welding) 반경 (센티미터 단위, 이 반경 내의 점들은 하나로 합쳐짐)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "1.0"))
	float WeldDistance;

	// 삼각형 변의 최대 허용 길이 (센티미터 단위, 도로 폭 이상의 거대한 불필요 삼각형 필터링용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "10.0"))
	float MaxEdgeLength;

	// 삼각형의 최소 내각 제한 (디그리 단위, 너무 좁고 긴 바늘 모양 삼각형 제거용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MinAngleDegree;

	// Z축 고도 오프셋 (지형 메쉬와의 Z-fighting 깜빡임 방지용 센티미터 단위 오프셋)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings")
	float ZOffset;

	// 공간 분할 격자 크기 (센티미터 단위, 대규모 정점의 삼각분할 연산 속도 개선 및 왜곡 방지용 격자 크기)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Mesh Settings", meta = (ClampMin = "500.0"))
	float GridSize;

	// 메쉬 생성 결과를 구워낼 타겟 Dynamic Mesh Actor (월드 상의 액터)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	ADynamicMeshActor* OutputDynamicMeshActor;

	// Static Mesh로 구울 때 저장할 패키지 패스 (예: /Game/HDMap/SM_NamsanRoad)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Output")
	FString SaveAssetPath;

	// 도로 노면 메쉬 생성 기능 호출 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void GenerateRoadMesh();

	// 생성된 메쉬를 Static Mesh 에셋으로 저장하는 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void SaveToStaticMeshAsset();

private:
	// 2D 델로네 삼각분할을 수행하는 핵심 헬퍼 함수
	bool RunDelaunayTriangulation(const TArray<FVector>& Vertices, TArray<FIntVector>& OutTriangles);

	// 두 2D 평면 벡터 사이의 거리 측정용 헬퍼
	float GetDistance2D(const FVector& V1, const FVector& V2) const;

	// 다각형 내부 판정용 고속 레이캐스팅 함수 (2D 투영)
	static bool IsPointInPolygon2D(const FVector2D& Point, const TArray<FVector>& PolygonPoints);

	// 레이트레이싱을 통해 월드 지형의 Z 고도값 탐색
	float GetLandscapeZ(const FVector& WorldPos);
};

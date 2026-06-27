// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HDMapStructs.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HDMapVisualizer.generated.h"

UCLASS()
class SHARDSOFSEOUL_API AHDMapVisualizer : public AActor
{
	GENERATED_BODY()
	
public:	
	AHDMapVisualizer();

protected:
	virtual void BeginPlay() override;

public:
	// 원본 GIS 파일셋이 있는 폴더 경로
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Import")
	FDirectoryPath SourceGISFolder;

	// 정밀도로지도 JSON 데이터가 임포트된 데이터 테이블 (Legacy)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data (Legacy)")
	UDataTable* HDMapDataTable;

	// 9개 레이어별 데이터테이블 에셋들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_A1_Node;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_A2_Link;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_A3_Driveway;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_A4_Subsidiary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_Sidewalk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_B2_Lane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_B3_Mark;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_C1_Light;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_C3_Protection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_C4_SpeedBump;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_C5_Barrier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_C6_Post;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Data")
	UDataTable* DT_Building;


	// 도로 링크(A2_LINK) 시각화에 사용할 스태틱 메쉬 (기본값: 피라미드/화살표머리 권장)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	UStaticMesh* LinkStaticMesh;

	// 차선 및 구조물 시각화에 사용할 스태틱 메쉬 (기본값: 박스/실린더/화살표몸통 권장)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	UStaticMesh* LaneStaticMesh;

	// 마디 및 교차점 정점 시각화에 사용할 구체 메쉬 (기본값: Sphere)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	UStaticMesh* SphereStaticMesh;

	// 5색 머티리얼 에셋들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	UMaterialInterface* LinkRedMaterial; // 빨강 : 주행경로 A2_LINK

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	UMaterialInterface* LaneGreenMaterial; // 초록 : 차선 B2_SURFACELINEMARK

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	UMaterialInterface* CurbPinkMaterial; // 분홍 : 연석/가드레일 C3_VEHICLEPROTECTIONSAFETY

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	UMaterialInterface* SidewalkYellowMaterial; // 노랑 : 보도 A4_SUBSIDIARYSECTION

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	UMaterialInterface* MarkBlueMaterial; // 파랑 : 횡단보도 및 노면표시 B3_SURFACEMARK / C4_SPEEDBUMP

	// 뷰포트 배치 시 샘플링 거리 간격 (센티미터 단위, 이 거리마다 화살표 인스턴스 배치)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style", meta = (ClampMin = "50.0"))
	float PointSpacing;

	// 화살표/인스턴스 메쉬 스케일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap - Style")
	FVector InstanceScale;

	// GIS 폴더 속성 데이터 변환 및 데이터테이블 자동 이식 실행 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void ImportHDMapData();

	// 시각화 데이터 테이블 파싱 및 뷰포트 드로잉 실행 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void VisualizeHDMap();

	// 시각화된 인스턴스 메쉬 컴포넌트 일괄 삭제 버튼
	UFUNCTION(CallInEditor, Category = "HDMap - Actions")
	void ClearVisualization();

	// 외부(MeshGenerator)에서 선택된 라인의 포인트 배열을 가져오기 위한 헬퍼 함수
	UFUNCTION(BlueprintCallable, Category = "HDMap - Utility")
	bool GetPointsByLineID(const FString& LineID, TArray<FVector>& OutPoints) const;

	// 현재 시각화된 모든 Line ID 목록 반환
	UFUNCTION(BlueprintCallable, Category = "HDMap - Utility")
	TArray<FString> GetAvailableLineIDs() const;

private:
	// 내부적으로 생성된 InstancedStaticMeshComponent들을 저장 및 추적
	UPROPERTY()
	TMap<FString, UInstancedStaticMeshComponent*> CreatedComponents;

	// 포인트 세그먼트 위에 화살표(피라미드+큐브+구체) 인스턴스를 그리는 내부 함수
	void DrawLineSegment(UInstancedStaticMeshComponent* ISMCompHead, UInstancedStaticMeshComponent* ISMCompBody, UInstancedStaticMeshComponent* ISMCompJoint, const TArray<FVector>& Points);
};

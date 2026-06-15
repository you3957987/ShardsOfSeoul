// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "HDMapStructs.generated.h"

/**
 * 정밀도로지도(HDMap)의 각 라인(링크, 차선) 데이터를 담는 데이터 테이블 구조체
 */
USTRUCT(BlueprintType)
struct FHDMapLineRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapLineRow()
		: ID(TEXT(""))
		, Type(TEXT(""))
	{}
};

/** A1_NODE: 교차점 노드 */
USTRUCT(BlueprintType)
struct FHDMapA1NodeRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString NodeType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapA1NodeRow() : ID(TEXT("")), NodeType(TEXT("")) {}
};

/** A2_LINK: 도로 링크 (중심선) */
USTRUCT(BlueprintType)
struct FHDMapA2LinkRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString LinkType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	int32 MaxSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString R_LinkID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString L_LinkID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapA2LinkRow() : ID(TEXT("")), LinkType(TEXT("")), MaxSpeed(0), R_LinkID(TEXT("")), L_LinkID(TEXT("")) {}
};

/** A3_DRIVEWAYSECTION: 차도 구간 */
USTRUCT(BlueprintType)
struct FHDMapA3DrivewayRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapA3DrivewayRow() : ID(TEXT("")), Kind(TEXT("")) {}
};

/** A4_SUBSIDIARYSECTION: 보도 및 부속 구간 */
USTRUCT(BlueprintType)
struct FHDMapA4SubsidiaryRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapA4SubsidiaryRow() : ID(TEXT("")), Kind(TEXT("")) {}
};

/** B2_SURFACELINEMARK: 차선 규제선 */
USTRUCT(BlueprintType)
struct FHDMapB2LaneRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString LaneType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString Color;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapB2LaneRow() : ID(TEXT("")), LaneType(TEXT("")), Color(TEXT("")), Kind(TEXT("")) {}
};

/** B3_SURFACEMARK: 노면 기호 표시 */
USTRUCT(BlueprintType)
struct FHDMapB3MarkRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapB3MarkRow() : ID(TEXT("")), Kind(TEXT("")) {}
};

/** C1_TRAFFICLIGHT: 신호등 */
USTRUCT(BlueprintType)
struct FHDMapC1LightRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString LightType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapC1LightRow() : ID(TEXT("")), LightType(TEXT("")) {}
};

/** C3_VEHICLEPROTECTIONSAFETY: 차량 방호 안전 시설 (연석, 가드레일 등) */
USTRUCT(BlueprintType)
struct FHDMapC3ProtectionRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ProtectionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	int32 IsCentral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapC3ProtectionRow() : ID(TEXT("")), ProtectionType(TEXT("")), IsCentral(0) {}
};

/** C4_SPEEDBUMP: 과속 방지턱 */
USTRUCT(BlueprintType)
struct FHDMapC4SpeedBumpRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapC4SpeedBumpRow() : ID(TEXT("")) {}
};

/** C5_HEIGHTBARRIER: 높이제한장해물 */
USTRUCT(BlueprintType)
struct FHDMapC5BarrierRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString BarrierType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	float Height;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapC5BarrierRow() : ID(TEXT("")), BarrierType(TEXT("")), Height(0.0f) {}
};

/** C6_POSTPOINT: 기둥 */
USTRUCT(BlueprintType)
struct FHDMapC6PostRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString PostType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapC6PostRow() : ID(TEXT("")), PostType(TEXT("")) {}
};

/** 인도 병합 레이어 구조체 (ID, UFID, Points) */
USTRUCT(BlueprintType)
struct FHDMapSidewalkRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	FString UFID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HDMap")
	TArray<FVector> Points;

	FHDMapSidewalkRow() : ID(TEXT("")), UFID(TEXT("")) {}
};


#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSVLogType.h" // <-- 구조체 인식을 위해 필수!
#include "CSVLog.generated.h"

UCLASS()
class LOGDATA_API UCSVLog : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 전투 기록(CSV) 데이터를 저장합니다.
	 * 
	 * @param EnemyID       개체의 고유 식별 코드 (예: "Rat_102")
	 * @param EnemyType     몬스터 종류 (예: "Melee")
	 * @param LogData       대미지, 시간 등 세부 데이터 구조체
	 * @param LogCategory   저장할 폴더 경로 카테고리 (예: "v1.0_Default")
	 * 로그 카테고리랑 ID, 타입은 필수로 지정해줘야 하고 로그 데이터는 넣을꺼만 만들어서
	 * !!!!!!! ID 랑 타입 넘기는 이유는 저걸 그냥 비긴 플레이나 컨스트럭트에서 정의하고
	 * 들어가면 슬라임이나 스포너, 이런걸로 스폰하는 경우에 주인_스폰 같이 ID 정의 불가능
	 * -> 걍 함수 파라미터로 마무리할 시기에 넘기는게 좋다!
	 */
	UFUNCTION(BlueprintCallable, Category = "LogData_CSVLogger", 
		Meta = (AutoCreateRefTerm = "LogData", AdvancedDisplay = "LogCategory")) 
	static void AddEnemyLog(
		const FString& LogCategory,
		const FString& EnemyID, 
		const FString& EnemyType, 
		const FEnemyLogData& LogData
	);
	
	
	/** 스켈레톤 마법사 보스 로그 */
	UFUNCTION(BlueprintCallable, Category = "LogData_CSVLogger", Meta = (AutoCreateRefTerm = "LogData"))
	static void AddSkeletonMageLog(
		const FString& LogCategory, 
		const FBossSkeletonMageLogData& LogData
		
	);
	
	// 매직소드맨 보스 로그
	UFUNCTION(BlueprintCallable, Category = "LogData_CSVLogger", Meta = (AutoCreateRefTerm = "LogData"))
	static void AddMagicSwordManLog(
		const FString& LogCategory, 
		const FBossMagicSwordManLogData& LogData
		
	);


};
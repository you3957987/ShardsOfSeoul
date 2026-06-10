#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemDropInterface.generated.h"

// 데이터 테이블에서 적 ID 안에 들어갈 내용
USTRUCT(BlueprintType)
struct FEnemyDropItemInfo
{
	GENERATED_BODY()

	// 스폰할 아이템의 블루프린트 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropInfo")
	TSubclassOf<AActor> DropItemClass;

	// 최초 1회만 드롭할 것인지(세이브 연동용) 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropInfo")
	bool bIsOneTimeDrop = false;
	
	// 아이템 드롭 개수를 최소~최대 범위 내에서 무작위로 결정할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropInfo")
	bool bUseRandomDropAmount = false;

	// 무작위 드롭 개수 적용 시 최소 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropInfo", meta = (EditCondition = "bUseRandomDropAmount"))
	int32 DropAmountMin = 1;

	// 무작위 드롭 개수 적용 시 최대 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropInfo", meta = (EditCondition = "bUseRandomDropAmount"))
	int32 DropAmountMax = 3;
	
	// 고정 드롭 시 아이템의 개수 (bUseRandomDropAmount가 false일 때 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropInfo", meta = (EditCondition = "!bUseRandomDropAmount"))
	int32 DropAmount = 1;
	
	// 개별적으로 돌아가는 아이템 드롭 확률[ 0 ~ 1 ] ( ex) 개수 3개면 1개 1개 1개씩 확률 적용 )
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropInfo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropChance = 1.0f; 
};

// 데이터 테이블
USTRUCT(BlueprintType)
struct FEnemyDropData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 행 이름을 ID로 하고 ID에 해당하는 드롭할 아이템
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DropInfo")
	TArray<FEnemyDropItemInfo> DropItems;
};

UINTERFACE(MinimalAPI)
class UItemDropInterface : public UInterface
{
	GENERATED_BODY()
};

class ENEMY_API IItemDropInterface
{
	GENERATED_BODY()

public:
	
	// 적에서 죽을때 타깃 캐릭터 인터페이스 확인 후 플레이어에서 구현한 함수 실행시킴
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemDrop")
	void HandleEnemyDeadAndDropItem( AActor* DeadActor );
	
	// 플레이어가 죽인 적 ID 가져올 수 있는 함수 -> 적에서 구현하며 단순히 return EnemyID; 가 끝
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ItemDrop")
	FName GetItemDropTableEnemyID() const;
	
};

#include "EnemyBoss/SkeletonMage/BTTask_SummonEnemy.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "EnemyBoss/SkeletonMage/BossSkeletonMage.h"

UBTTask_SummonEnemy::UBTTask_SummonEnemy()
{
	NodeName = "SummonEnemy";
}

EBTNodeResult::Type UBTTask_SummonEnemy::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	// AI 컨트롤러 가져오기
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;
	
	// 컨트롤러가 제어하는 폰을 ABossSkeletonMage로 캐스팅
	ABossSkeletonMage* BossPawn = Cast<ABossSkeletonMage>(AIController->GetPawn());
	if (!BossPawn) return EBTNodeResult::Failed;
	
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem) return EBTNodeResult::Failed;
	
	TArray<FVector> SummonLocations;
	const TArray<USceneComponent*> OriginPoints = { BossPawn->SummonPointOne, BossPawn->SummonPointTwo };
	const int32 MaxAttemptsPerPoint = 10;

	// 각 기준점(SummonPointOne, SummonPointTwo)에 대해 위치를 찾습니다.
	for (USceneComponent* OriginPoint : OriginPoints)
	{
		bool bLocationFound = false;
		for (int32 i = 0; i < MaxAttemptsPerPoint; ++i)
		{
			FNavLocation NavLocation;
			// MaxSummonDist 반경 내에서 이동 가능한 랜덤 위치를 찾습니다.
			if (NavSystem->GetRandomReachablePointInRadius(
				OriginPoint->GetComponentLocation(),
				BossPawn->AttackStruct.MaxSummonDist,
				NavLocation))
			{
				SummonLocations.Add(NavLocation.Location);
				bLocationFound = true;
				break; // 위치를 찾았으므로 다음 기준점으로 넘어갑니다.
			}
		}

		// 특정 기준점에 대한 위치 찾기에 실패하면 전체 작업을 실패 처리합니다.
		if (!bLocationFound)
		{
			return EBTNodeResult::Failed;
		}
	}

	// 2개의 유효한 위치를 모두 찾았는지 확인합니다.
	if (SummonLocations.Num() == 2)
	{
		BossPawn->StartSummoning(SummonLocations[0], SummonLocations[1]);
		return EBTNodeResult::Succeeded;
	}
	
	return EBTNodeResult::Failed;
}

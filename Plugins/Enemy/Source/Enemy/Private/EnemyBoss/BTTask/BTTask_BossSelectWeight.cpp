#include "EnemyBoss/BTTask/BTTask_BossSelectWeight.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/BaseBossEnemy.h"

UBTTask_BossSelectWeight::UBTTask_BossSelectWeight()
{
	NodeName = "Boss_Select_Weight";
}

EBTNodeResult::Type UBTTask_BossSelectWeight::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	APawn* ControlledPawn = AIController->GetPawn();
	if (!ControlledPawn) return EBTNodeResult::Failed;

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp) return EBTNodeResult::Failed;

	// 거리 계산
	FVector PlayerPos = BlackboardComp->GetValueAsVector(PlayerLocation.SelectedKeyName);
	FVector BossPos = ControlledPawn->GetActorLocation();
	float Distance = FVector::Dist(PlayerPos, BossPos);

	// 설정 적용을 위한 람다 함수 (중복 제거용)
	auto ApplyWeights = [&](const TArray<FBossSelectWeightConfig>& Settings)
	{
		for (const FBossSelectWeightConfig& Config : Settings)
		{
			if (Config.Key.SelectedKeyName != NAME_None)
			{
				BlackboardComp->SetValueAsFloat(Config.Key.SelectedKeyName, Config.Value);
			}
		}
	};

	// 보스 객체 가져오기
	ABaseBossEnemy* Boss = Cast<ABaseBossEnemy>(ControlledPawn);
	FString SelectedRangeName;
	
	if ( Boss )
	{
		// 거리별 분기 처리
		if (Distance <= CloseRangeDistance) // 근거리
		{
			ApplyWeights(CloseRangeWeights);
			// 로그
			UE_LOG(LogTemp, Warning, TEXT("Distance: %f - Close Range Weights Applied"), Distance);
			SelectedRangeName = TEXT("Short Range");
			Boss->CommonBossLogData.ShortRangePatternCount++; // 로그 데이터에 근거리 패턴 선택 횟수 누적
		}
		else if (Distance > CloseRangeDistance && Distance <= MidRangeDistance) // 중거리
		{
			ApplyWeights(MidRangeWeights);
			// 로그
			UE_LOG(LogTemp, Warning, TEXT("Distance: %f - Mid Range Weights Applied"), Distance);
			SelectedRangeName = TEXT("Mid Range");
			Boss->CommonBossLogData.MidRangePatternCount++; // 로그 데이터에 중거리 패턴 선택 횟수 누적
		}
		else // 원거리
		{
			ApplyWeights(FarRangeWeights);
			// 로그
			UE_LOG(LogTemp, Warning, TEXT("Distance: %f - Far Range Weights Applied"), Distance);
			SelectedRangeName = TEXT("Long Range");
			Boss->CommonBossLogData.LongRangePatternCount++; // 로그 데이터에 원거리 패턴 선택 횟수 누적
		}
		Boss->SelectedRangeName = SelectedRangeName; // 선택된 거리 범주 이름 저장
	}
	
	return EBTNodeResult::Succeeded;
}

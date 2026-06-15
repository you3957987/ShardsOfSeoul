#include "BTTask/BTTask_BattleStartLog.h"

#include "AIController.h"
#include "BaseEnemy.h"

UBTTask_BattleStartLog::UBTTask_BattleStartLog()
{
	NodeName = "BattleStartLog";

	bNotifyTick = false;
}

EBTNodeResult::Type UBTTask_BattleStartLog::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 1. AI 컨트롤러 가져오기
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	// 2. 제어 중인 폰(BaseEnemy) 가져오기
	ABaseEnemy* Enemy = Cast<ABaseEnemy>(AIController->GetPawn());
	if (Enemy)
	{
		// 3. BaseEnemy에 만들어둔 전투 시작 로그 함수 호출
		Enemy->StartBattleLog();
		
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

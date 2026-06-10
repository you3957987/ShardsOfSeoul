#include "EnemyBoss/BTService/BTService_BossSetMoveSpeed.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTService_BossSetMoveSpeed::UBTService_BossSetMoveSpeed()
{
	NodeName = TEXT("Set Move Speed");
	
	// 중요: 이 서비스를 사용하는 각 AI마다 별도의 인스턴스를 생성해야 
	// CachedMoveSpeed 변수가 꼬이지 않습니다. 
	bCreateNodeInstance = true; 
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	bNotifyTick = true; 
	
	Interval = 0.1f; 
}

void UBTService_BossSetMoveSpeed::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		ACharacter* OwnerCharacter = Cast<ACharacter>(AIController->GetPawn());
		if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
		{
			// 1. 현재 속도를 백업
			CachedMoveSpeed = OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed;

			// 2. 목표 속도로 변경
			OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed = TargetMoveSpeed;
			
		}
	}
}

void UBTService_BossSetMoveSpeed::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	APawn* OwningPawn = OwnerComp.GetAIOwner()->GetPawn();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (OwningPawn && Blackboard)
	{
		// 블랙보드에서 타겟(플레이어) 가져오기
		UObject* TargetObject = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
		AActor* TargetActor = Cast<AActor>(TargetObject);

		if (TargetActor)
		{
			// 거리 계산
			float Dist = FVector::Dist(OwningPawn->GetActorLocation(), TargetActor->GetActorLocation());

			// [핵심] 사거리 안에 들어오면 공격 가능 상태로 변경
			if (Dist <= AttackRange)
			{
				// CanAttack 키를 true로 설정
				Blackboard->SetValueAsBool(CanAttackKey.SelectedKeyName, true);
				
			}
		}
	}
}

void UBTService_BossSetMoveSpeed::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (AIController)
	{
		ACharacter* OwnerCharacter = Cast<ACharacter>(AIController->GetPawn());
		if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
		{
			// 3. 원래 속도로 복구
			OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed = CachedMoveSpeed;
		}
	}
}

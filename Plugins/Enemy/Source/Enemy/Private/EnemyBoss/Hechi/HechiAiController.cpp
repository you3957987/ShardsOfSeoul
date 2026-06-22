#include "EnemyBoss/Hechi/HechiAiController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyBoss/Hechi/Hechi.h"

void AHechiAiController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	ControlledEnemy = Cast<AHechi>(GetPawn());
}

void AHechiAiController::SetBlackboardKey()
{
	Super::SetBlackboardKey();
	
	
	if ( BlackboardComp == nullptr || ControlledEnemy == nullptr ) return;
	
	BlackboardComp->SetValueAsBool("CanAttack", true);
	BlackboardComp->SetValueAsFloat("AttackDelay", ControlledEnemy->AttackDelay);
	BlackboardComp->SetValueAsObject(TEXT("TargetCharacter"), TargetPawn);
	BlackboardComp->SetValueAsBool("ChangeMap", false);
	
	BlackboardComp->SetValueAsFloat("W_LaserAttack", ControlledEnemy->AttackStruct.LaserAttackWeight);
	BlackboardComp->SetValueAsFloat("W_GravityAttack", ControlledEnemy->AttackStruct.GravityAttackWeight);
	BlackboardComp->SetValueAsFloat("W_Teleport", ControlledEnemy->AttackStruct.TeleportWeight);
	BlackboardComp->SetValueAsFloat("W_ThrowMagicBall", ControlledEnemy->AttackStruct.ThrowMagicBallWeight);
	
}

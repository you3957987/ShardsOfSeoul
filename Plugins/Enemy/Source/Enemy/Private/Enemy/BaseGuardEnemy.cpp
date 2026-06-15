#include "Enemy/BaseGuardEnemy.h"

#include "EnemyLogManager.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseGuardEnemy::ABaseGuardEnemy()
{
	EnemyType = EEnemyType::EET_Guard; // 방패병 타입으로 설정
}

void ABaseGuardEnemy::BeginPlay()
{
	Super::BeginPlay();
	
	TArray<UCapsuleComponent*> CapsuleCollisionComps;
	GetComponents<UCapsuleComponent>(CapsuleCollisionComps);
	
	// 반복문 돌면서 태그 확인
	for (UCapsuleComponent* Capsule : CapsuleCollisionComps)
	{
		if (Capsule && Capsule->ComponentHasTag(TEXT("Weapon"))) // "Weapon" 태그를 가진 콜리전 스피어를 찾습니다.)))
		{
			WeaponCollision = Capsule;
			WeaponCollision->OnComponentBeginOverlap.AddDynamic(this, &ABaseGuardEnemy::OnBeginOverlapWeaponCollisionSphere);
			// 콜리전 끄기
			WeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break; // 찾았으니 루프 종료
		}
	}
	
	// ===========================================================================
	// [테스트용] 5초 뒤부터 1초 간격으로 5 대미지 자해 (몽타주 재생 확인용)
	// ===========================================================================
	/*
	// 람다 함수 정의
	FTimerDelegate DebugDamageDelegate;
	DebugDamageDelegate.BindLambda([this]()
	{
		if (IsValid(this))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Debug] Auto Damage: Applied 5.0 Damage to Self"));
			// 스스로에게 대미지 적용 (DamageCauser = this)
			UGameplayStatics::ApplyDamage(this, 
				5.0f, GetController(), this, UDamageType::StaticClass());
		}
	});

	// 타이머 핸들 (테스트용이라 static으로 선언하여 핸들 유지, 정석은 헤더에 선언)
	static FTimerHandle DebugTimerHandle;

	// 1초(Rate)마다 반복(Loop=true), 첫 실행은 5초 뒤(FirstDelay=5.0f)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DebugTimerHandle, DebugDamageDelegate, 1.0f, true, 5.0f);
	}
	*/
}

UAnimMontage* ABaseGuardEnemy::Attack()
{
	AttackDamage = NormalAttackDamage; // 공격 대미지를 일반 공격 대미지로 설정
	return Super::Attack();
}

float ABaseGuardEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	if ( bIsGuarding )
	{
		DamageWhileGuarding += DamageAmount;

		// 아직 리액션 대미지에 도달하지 않았으면 가드 몽타주 재생
		if ( DamageWhileGuarding < MaxDamageToReaction && GuardMontage )
		{
			PlayAnimMontage(GuardMontage);
		}
		// 로그 기록 로직
		if (GetMesh()) 
		{
			// 스켈레탈 메쉬 에셋 이름 가져오기
			FString MeshName = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
    
			// 현재 적용된 대미지가 기본 공격인지 가드 반격인지 구분
			FString AttackTypeName = (AttackDamage == GuardReactionDamage) ? TEXT("가드 반격") : TEXT("기본 공격");

			UEnemyLogManager::EnemyLog(EEnemyLogType::Guard, 
				FString::Printf(TEXT("적 [%s]가 [%.f] 대미지 가드 | 가드중 받은 대미지 / 반격 임계치[%.f / %.f]"), 
					*MeshName, DamageAmount, DamageWhileGuarding, MaxDamageToReaction));
		}
		
		EnemyLogData.TotalDamageGuarded += DamageAmount; // 로그 데이터에 가드한 대미지 누적
		
		return 0.f; // 대미지 무효화
	}
	
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ABaseGuardEnemy::GuardReactionAttack()
{
	if ( GuardReactionAttackMontage ) // 공격 애니메이션 몽타주가 설정되어 있는지 확인
	{
		PlayAnimMontage(GuardReactionAttackMontage); // 공격 애니메이션 재생
	}
	
	EnemyLogData.CounterAttackCount += 1; // 로그 데이터에 가드 반격 횟수 누적
	
	AttackDamage = GuardReactionDamage; // 공격 대미지를 가드 리액션 공격 대미지로 설정

	bFocusPlayerAfterAttack = false;
}

void ABaseGuardEnemy::OnBeginOverlapWeaponCollisionSphere(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor->ActorHasTag(FName("Player")))
	{
		// 어택 대미지 로그 
		UE_LOG(LogTemp, Warning, TEXT("Guard Enemy Attack Damage : %f"), AttackDamage);

		// 로그 기록 로직
		if (GetMesh()) 
		{
			// 스켈레탈 메쉬 에셋 이름 가져오기
			FString MeshName = GetMesh()->GetSkeletalMeshAsset() ? GetMesh()->GetSkeletalMeshAsset()->GetName() : TEXT("NoMeshAsset");
    
			// 현재 적용된 대미지가 기본 공격인지 가드 반격인지 구분
			FString AttackTypeName = (AttackDamage == GuardReactionDamage) ? TEXT("가드 반격") : TEXT("기본 공격");

			UEnemyLogManager::EnemyLog(EEnemyLogType::Guard, 
				FString::Printf(TEXT("적 [%s]가 [%s]으로 [%.f] 대미지"), 
					*MeshName, 
					*AttackTypeName,
					AttackDamage));
		}
		
		EnemyLogData.TotalDamageDealt += AttackDamage; // 로그 데이터에 입힌 대미지 누적
		
		// 대미지 적용 ( 어택 대미지는 공격 전 가드 공격인지 아님 일반 공격인지에 따라 각각 함수에서 설정 )
		UGameplayStatics::ApplyDamage(OtherActor, AttackDamage, GetController(),
			this, UDamageType::StaticClass());
		
		// 다시 콜리전 끄기
		if ( WeaponCollision ) WeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABaseGuardEnemy::AttackStart_WeaponCollision()
{
	if ( WeaponCollision ) WeaponCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ABaseGuardEnemy::AttackEnd_WeaponCollision()
{
	if ( WeaponCollision ) WeaponCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

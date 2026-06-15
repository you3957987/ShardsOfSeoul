#include "EnemyProjectile/GroundAttackProjectile.h"

#include "EnemyLogManager.h"
#include "EnemyBoss/SkeletonMage/BossSkeletonMage.h"
#include "Kismet/GameplayStatics.h"


AGroundAttackProjectile::AGroundAttackProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	RootComponent = RootComp;
	
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent); // 루트 컴포넌트에 부착
	
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComp->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
}

void AGroundAttackProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// 생성 후 상승 로직 시작
	if (RiseDuration > 0.f )
	{
		InitialLocation = GetActorLocation();
		bIsRising = true;
	}
}

void AGroundAttackProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsRising == true)
	{
		UpperMesh(DeltaTime);
	}
	else if (bIsStaying == true)
	{
		StayElapsedTime += DeltaTime;
		if (StayElapsedTime >= DurationTime)
		{
			bIsStaying = false;
			// 하강 시간이 있으면 하강 시작, 없으면 바로 파괴
			if (RiseDuration > 0.f)
			{
				bIsLowering = true;
			}
			else
			{
				Destroy();
			}
		}
	}
	else if (bIsLowering == true)
	{
		//UE_LOG(LogTemp, Warning, TEXT("aaa"));
		LowerMesh(DeltaTime);
	}
}

void AGroundAttackProjectile::UpperMesh(float DeltaTime)
{
	if (bIsRising == true)
	{
		RiseElapsedTime += DeltaTime;
		if (RiseElapsedTime < RiseDuration)
		{
			// 시간에 따라 목표 높이까지 상승
			const float NewZ = InitialLocation.Z + RiseHeight * (RiseElapsedTime / RiseDuration);
			FVector NewLocation = GetActorLocation();
			NewLocation.Z = NewZ;
			SetActorLocation(NewLocation);
			//UE_LOG(LogTemp, Warning, TEXT("aaaa"));
		}
		else
		{
			// 상승 완료
			bIsRising = false;
			// 최종 위치 설정
			FVector FinalLocation = GetActorLocation();
			FinalLocation.Z = InitialLocation.Z + RiseHeight;
			SetActorLocation(FinalLocation);

			HandleDamage();
			
			// 최종 위치를 PeakLocation에 저장합니다.
			PeakLocation = InitialLocation + FVector(0.f, 0.f, RiseHeight);
			bIsStaying = true;
		}
	}
}

void AGroundAttackProjectile::LowerMesh(float DeltaTime)
{
	LowerElapsedTime += DeltaTime;
	if (LowerElapsedTime < LowerDuration)
	{
		// Lerp의 Alpha 값을 계산할 때 RiseDuration 대신 LowerDuration을 사용합니다.
		const float NewZ = FMath::Lerp(PeakLocation.Z, InitialLocation.Z, LowerElapsedTime / LowerDuration);
		FVector NewLocation = GetActorLocation();
		NewLocation.Z = NewZ;
		SetActorLocation(NewLocation);
	}
	else
	{
		bIsLowering = false;
		SetActorLocation(InitialLocation);
		//UE_LOG(LogTemp, Warning, TEXT("Ground Attack Projectile Lowering Complete"));
		Destroy(); // 하강 완료 후 액터 파괴
	}
}

void AGroundAttackProjectile::HandleDamage()
{
	if (!MeshComp) return;

	// 1. 현재 MeshComp와 겹쳐 있는 모든 액터를 가져옵니다.
	TArray<AActor*> OverlappingActors;
	MeshComp->GetOverlappingActors(OverlappingActors);

	for (AActor* OtherActor : OverlappingActors)
	{
		// 2. 자기 자신이나 소유자 제외, 유효성 검사
		if (OtherActor && (OtherActor != this) && (OtherActor != GetOwner()))
		{
			// 3. "Player" 태그 확인
			if (OtherActor->ActorHasTag(FName("Player")))
			{
				
				ABossSkeletonMage* BossOwner = Cast<ABossSkeletonMage>(GetOwner());
					
				if ( BossOwner )
				{
					BossOwner->BossSkeletonMageLogData.GroundAttackDamage += Damage; // 로그 데이터에 입힌 대미지 누적
					BossOwner->CommonBossLogData.TotalDamageDealt += Damage; // 공통 로그 데이터에도 누적
				}
				
				UEnemyLogManager::EnemyLog(EEnemyLogType::SkeletonMage, 
					FString::Printf(TEXT("[스켈레톤 메이지] 뼈 장판 공격 [%.f] 대미지 줌"), Damage));
                
				// 4. 대미지 적용
				UGameplayStatics::ApplyDamage(
					OtherActor,
					Damage, 
					GetOwner() ? GetOwner()->GetInstigatorController() : nullptr,
					this,
					UDamageType::StaticClass()
				);
                
				// 5. 단발성 공격이라면 콜리전 비활성화 (필요 시)
				MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				break; // 한 명의 플레이어만 공격한다면 루프 종료
			}
		}
	}
}


#include "BaseFlyingPet.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Component/PetTalkComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Header/PetState.h"

ABaseFlyingPet::ABaseFlyingPet()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	RootComponent = CollisionComp;
	// 카메라 채널 무시 설정
	CollisionComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);
	// 메쉬는 충돌을 처리하지 않음
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));

	// 적 감지 범위 구체 컴포넌트 생성 및 설정
	EnemyDetectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("EnemyDetectSphere"));
	EnemyDetectSphere->SetupAttachment(RootComponent);
	EnemyDetectSphere->ShapeColor = FColor::Green;
	EnemyDetectSphere->SetSphereRadius(EnemyDetectRange);
	EnemyDetectSphere->SetVisibility(false);
	EnemyDetectSphere->SetHiddenInGame(false); 

	// 아이템 감지 범위 구체 컴포넌트 생성 및 설정
	ItemDetectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ItemDetectSphere"));
	ItemDetectSphere->SetupAttachment(RootComponent);
	ItemDetectSphere->ShapeColor = FColor::Blue;
	ItemDetectSphere->SetSphereRadius(ItemDetectRange);
	ItemDetectSphere->SetVisibility(false);
	ItemDetectSphere->SetHiddenInGame(false);

	// 아이템 감지 핑 생성 위치 컴포넌트
	ItemDetectPingSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ItemDetectPingSpawnPoint"));
	ItemDetectPingSpawnPoint->SetupAttachment(RootComponent);
	
	// 펫 대화 컴포넌트 생성
	PetTalkComp = CreateDefaultSubobject<UPetTalkComponent>(TEXT("PetTalkComp"));
	
	// 펫 태그 추가
	Tags.Add("Pet");
}

void ABaseFlyingPet::BeginPlay()
{
	Super::BeginPlay();
	
	if (PetTalkComp)
	{
		// 대화가 끝나면 내 클래스의 EndConversation 함수를 실행해라! 라고 등록
		PetTalkComp->OnConversationEnded.AddDynamic(this, &ABaseFlyingPet::EndConversation);
	}

	// [추가] 외부에서 OnPetConversationStart를 Broadcast하면 자동으로 StartConversation이 실행되도록 연결
	OnPetConversationStart.AddDynamic(this, &ABaseFlyingPet::StartBigConversation);
	
	// 일정 주기마다 주변 적 감지 함수 호출 설정
	GetWorld()->GetTimerManager().SetTimer(
		EnemyDetectTimerHandle, 
		this, 
		&ABaseFlyingPet::CheckSurroundingEnemy, 
		EnemyDetectInterval, 
		true // 반복 여부: true
	);
	
	GetWorld()->GetTimerManager().SetTimer(
		LineOfSightTimerHandle, 
		this, 
		&ABaseFlyingPet::CheckLineOfSightToTarget, 
		2.0f, // 2초 간격
		true  // 반복 여부
	);
	
	// 3초 마다 아이템 감지
	GetWorld()->GetTimerManager().SetTimer(
		ItemDetectTimerHandle, 
		this, 
		&ABaseFlyingPet::CheckSurroundingItems, 
		3.0f, 
		true
	);
}

void ABaseFlyingPet::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PollInit(DeltaTime);

	if ( TargetActor && bIsFolloingTarget == true )
	{
		FollowingTarget(DeltaTime);
	}
}

void ABaseFlyingPet::PollInit(float DeltaTime)
{
	if (bTargetInitalize == false)
	{
		TargetActor = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
       
		if (TargetActor) 
		{
			// ✅ 인터페이스를 통해 함수 호출
			if (TargetActor->GetClass()->ImplementsInterface(UPetConversationInterface::StaticClass()))
			{
				// Execute_ 함수명을 사용하여 안전하게 호출합니다.
				IPetConversationInterface::Execute_SetMyPet(TargetActor, this);
            
				bTargetInitalize = true;
				bIsFolloingTarget = true;
			}
		}
	}
}

void ABaseFlyingPet::SetFreeRoaming(bool bNewState)
{
	bIsFolloingTarget = bNewState;
}

void ABaseFlyingPet::FollowingTarget(float DeltaTime)
{
	if (!TargetActor) return;

	// 상태(PetState)에 따른 설정값 선택 
	FPetPositionSettings CurrentSettings;
	switch (PetState)
	{
	case EPetState::EPS_Conversation:
		CurrentSettings = ConversationSettings;
		break;
	case EPetState::EPS_Follow:
		CurrentSettings = FollowSettings;
		break;
	default:
		CurrentSettings = FollowSettings;
		break;
	}

	FVector CurrentLocation = GetActorLocation();
	FVector TargetLocation = TargetActor->GetActorLocation();

	// --- 1. 목표 위치 계산 ---
	FVector TargetForward = TargetActor->GetActorForwardVector();
	FVector TargetRight = TargetActor->GetActorRightVector();

	// 공식 == 타겟 위치 - (앞방향 * 거리) + (오른쪽방향 * 좌우오프셋) + (위방향 * 높이오프셋)
	FVector DesiredLocation = TargetLocation
		- (TargetForward * CurrentSettings.Distance)    // 거리
		+ (TargetRight * CurrentSettings.SideOffset)    // 좌우
		+ FVector(0.0f, 0.0f, CurrentSettings.UpOffset); // 높이
	
	// 이동 속도 계산 (기존 로직 유지)
	float DistanceToTarget = FVector::Dist(CurrentLocation, DesiredLocation);
	float MoveSpeedMultiplier = 1.0f + (DistanceToTarget * 0.040f);
	float FinalInterpSpeed = MoveInterpSpeed * MoveSpeedMultiplier;
    
	// 보간된 새로운 위치 계산
	FVector NewLocation = FMath::VInterpTo(CurrentLocation, DesiredLocation, DeltaTime, FinalInterpSpeed);

	// [핵심 변경 사항] bSweep을 true로 설정하여 충돌 감지 활성화
	FHitResult Hit;
	bool bMoved = SetActorLocation(NewLocation, true, &Hit);

	// 만약 벽에 부딪혔다면 (충돌이 발생했다면)
	if (Hit.IsValidBlockingHit())
	{
		// 벽을 타고 미끄러지는 이동(Slide)을 추가하여 자연스럽게 함
		FVector RemainingDelta = NewLocation - Hit.Location;
		FVector SlideDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);
		AddActorWorldOffset(SlideDelta, true);
	}
    
	// 속도 계산 로직 유지
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		CurrentVelocity = (GetActorLocation() - CurrentLocation) / DeltaTime;
	}
	
	// --- 2. 회전 계산 (상태에 따른 분기) ---
	FRotator TargetRotation;

	if (PetState == EPetState::EPS_Conversation)
	{
		// [대화 상태] 펫이 캐릭터를 바라보도록 회전 계산 (LookAt)
		TargetRotation = UKismetMathLibrary::FindLookAtRotation(CurrentLocation, TargetLocation);
	}
	else
	{
		TargetRotation = TargetActor->GetActorRotation();
	}

	// [수정] 회전 속도도 각도 차이에 따라 선형 증가
	FRotator CurrentRotation = GetActorRotation();
	float DeltaYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw));

	// !!!!!!!!!!!! 각도 차이가 클수록 회전 속도 선형 증가 (예: 10도 차이날 때마다 0.5배씩 증가 등)
	float RotSpeedMultiplier = 1.0f + (DeltaYaw * 0.10f);
	float FinalRotationSpeed = MoveInterpSpeed * RotSpeedMultiplier;

	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, FinalRotationSpeed);
	SetActorRotation(NewRotation);
}

void ABaseFlyingPet::CheckLineOfSightToTarget()
{
	if (!TargetActor) return;

	FHitResult Hit;
	FVector Start = GetActorLocation();
	FVector End = TargetActor->GetActorLocation();
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(TargetActor);

	// LineTrace 등을 사용하여 시야 방해물이 있는지 확인하는 로직이 들어갈 자리입니다.
	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	
	if ( bHit == true )
	{
		LineOfSightBlockedCount++;
		
		if (LineOfSightBlockedCount >= 2)
		{
			FVector TargetLocation = TargetActor->GetActorLocation();
			FVector TargetForward = TargetActor->GetActorForwardVector();
			FVector TargetRight = TargetActor->GetActorRightVector();

			// 1. 가고 싶은 이상적인 위치 (FollowSettings 기준)
			FVector DesiredLocation = TargetLocation
				- (TargetForward * FollowSettings.Distance)
				+ (TargetRight * FollowSettings.SideOffset)
				+ FVector(0.0f, 0.0f, FollowSettings.UpOffset);

			// 2. 캐릭터에서 목표 지점으로 레이를 쏴서 "실제로 갈 수 있는 끝점" 찾기
			FHitResult SweepHit;
			FVector TraceStart = TargetLocation + FVector(0.0f, 0.0f, FollowSettings.UpOffset);
			FVector FinalLocation = DesiredLocation;

			if (GetWorld()->LineTraceSingleByChannel(SweepHit, TraceStart, DesiredLocation, ECC_Visibility, Params))
			{
				// 벽에 부딪혔다면, 그 충돌 지점에서 살짝 안쪽으로 들어온 위치를 최종 목적지로 설정
				FinalLocation = SweepHit.Location + (SweepHit.Normal * 20.0f);
			}

			// 3. 순간이동 실행 (이때는 이미 안전한 위치를 계산했으므로 bSweep은 false여도 됨)
			SetActorLocation(FinalLocation, false, nullptr, ETeleportType::TeleportPhysics);
			
			// 로그 확인 (선택 사항)
			UE_LOG(LogTemp, Warning, TEXT("Pet Teleported: Line of sight blocked by %s"), *Hit.GetActor()->GetName());
			
			LineOfSightBlockedCount = 0; // 순간이동 후 카운트 초기화
			// 순간이동 이펙트 재생
			if (TeleportEffect)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TeleportEffect, 
					FinalLocation, FRotator::ZeroRotator);
			}
		}
	}
	else LineOfSightBlockedCount = 0; // 시야가 확보된 경우 카운트 초기화
}

void ABaseFlyingPet::CheckSurroundingEnemy()
{
	// 현재 상태가 대화 중이면 적 감지 무시
	if (!EnemyDetectSphere || PetState == EPetState::EPS_Conversation) return;
	
	TArray<AActor*> OverlappingActors;
	// 현재 스피어 안에 있는 모든 액터를 가져옵니다. (필터링할 클래스가 있다면 두 번째 인자에 넣음)
	EnemyDetectSphere->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());

	bool bEnemyFound = false;

	for (AActor* Actor : OverlappingActors)
	{
		// 나 자신은 제외
		if (Actor == this) continue;

		// 적 태그 확인 (혹은 인터페이스나 클래스 캐스팅 확인)
		if (Actor->ActorHasTag("Enemy"))
		{
			// 배틀 상태가 아니면 타겟과의 시야 체크를 해서 벽으로 가려진 적은 무시하도록 (배틀 상태에서는 일단 감지된 적은 모두 적으로 간주)
			if ( PetState != EPetState::EPS_Battle && TraceCharacterToTarget( Actor ) == false ) continue;
			
			// 미믹 몬스터 또는 버로우 몬스터는 무시
			if (Actor->ActorHasTag("Mimic") || Actor->ActorHasTag("Burrow") 
				|| Actor->ActorHasTag("Trans")) continue;
			
			// Boss 태그가 있다면 무시 
			if (Actor->ActorHasTag("Boss"))
			{
				if (bBossBattleMode == false)
				{
					bBossBattleMode = true;
					EnemyDetectSphere->SetSphereRadius(30000.f);
					
					HealCharacter(); // 보스전 시작 시 캐릭터 힐링
					
					UE_LOG(LogTemp, Warning, TEXT("BossBattleMode Detected"));
				}
				bEnemyFound = true; // ◀ 보스도 적으로 간주하여 상태 업데이트 로직으로 넘어가게 함
				break; // 보스를 찾았으니 반복문 탈출
			}
			
			bEnemyFound = true;
			break; // 한 명이라도 있으면 배틀 모드이므로 더 검사할 필요 없음
		}
	}

	// 상태 업데이트
	if (bEnemyFound == true )
	{
		if (PetState != EPetState::EPS_Battle)
		{
			// 평상시 상태였는데 주변 적이 있다는 의미
			PetState = EPetState::EPS_Battle;
			//bIsFolloingTarget = false; // 배틀 모드 진입 시 자유 이동 모드로 전환
			
			if ( bBossBattleMode == true && PetTalkComp ) PetTalkComp->Travel_FollowToBattle( true );
			else if ( bBossBattleMode == false && PetTalkComp ) PetTalkComp->Travel_FollowToBattle( false );
		}
	}
	else if ( bEnemyFound == false && bBossBattleMode == true ) // 보스전 모드 해제 조건
	{
		PetState = EPetState::EPS_Follow;
		if ( PetTalkComp ) PetTalkComp->Travel_BattleToFollow( true );
		UE_LOG(LogTemp, Warning, TEXT("BossBattleMode Ended - EnemyDetectRange Restore"));
		bBossBattleMode = false;
		//bIsFolloingTarget = true; // 다시 따라다니기 모드로 전환
		EnemyDetectSphere->SetSphereRadius(EnemyDetectRange); // 원래 범위으로 복귀	
	}
	else
	{
		if (PetState == EPetState::EPS_Battle) 
		{
			// 배틀 모드에서 벗어나 평상시 상태로 복귀
			PetState = EPetState::EPS_Follow;
			if ( PetTalkComp ) PetTalkComp->Travel_BattleToFollow( false );
			//bIsFolloingTarget = true; // 다시 따라다니기 모드로 전환
		}
	}
}

void ABaseFlyingPet::HealCharacter()
{
	if (TargetActor)
	{
		// TargetActor 한테 - 대미지 주기 
		UGameplayStatics::ApplyDamage(TargetActor, -HealAmount, 
			nullptr, this, nullptr);
		
		// 2. 나이아가라 이펙트를 그 자리(Location)에 생성
		if (HealEffect)
		{
			// SpawnSystemAtLocation을 사용하면 부착되지 않고 월드 좌표에 고정되어 실행됩니다.
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				HealEffect,
				TargetActor->GetActorLocation(), // 플레이어의 현재 위치(고정값)
				FRotator::ZeroRotator,
				FVector(2.f),                    // 스케일
				true                         // 자동 파괴
			);
		}
	}
}

bool ABaseFlyingPet::TraceCharacterToTarget(AActor* Target)
{
	if (!TargetActor || !Target) return false;

	FHitResult Hit;
	FVector Start = TargetActor->GetActorLocation() + (FVector::UpVector * 50.f);
	FVector End = Target->GetActorLocation() + (FVector::UpVector * 50.f); 
	
	// 캐릭터의 눈높이 정도를 고려한다면 높이 오프셋을 추가할 수 있습니다.
	// Start.Z += 50.f; 
	// End.Z += 50.f;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);          // 펫 무시
	Params.AddIgnoredActor(TargetActor);   // 플레이어 무시
	Params.AddIgnoredActor(Target);        // 타겟 무시 (장애물만 체크하기 위함)

	// ECC_Visibility 채널을 사용하여 가시성을 가로막는 물체(벽 등)가 있는지 검사
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, 
		Start, 
		End, 
		ECC_Visibility, 
		Params
	);
	
	if (bHit)
	{
		// 무엇에 부딪혔는지 로그 출력
		UE_LOG(LogTemp, Warning, TEXT("Trace Blocked by: %s"), *Hit.GetActor()->GetName());
		//DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 1.0f, 0, 2.0f);
	}

	// 무언가에 부딪혔다면 시야가 가려진 것이므로 false 리턴
	return !bHit;
}

void ABaseFlyingPet::CheckSurroundingItems()
{
	// 따라다니기 상태가 아니면 무시
	if (PetState != EPetState::EPS_Follow || !ItemDetectSphere) return;

	TArray<AActor*> OverlappingActors;
	// 범위 내의 모든 액터를 가져옴
	ItemDetectSphere->GetOverlappingActors(OverlappingActors);

	for (AActor* Actor : OverlappingActors)
	{
		if (!Actor || Actor == this) continue;

		// 태그 확인 (Item 또는 Mimic)
		bool bIsItem = Actor->ActorHasTag("Item");
		bool bIsMimic = Actor->ActorHasTag("Mimic");

		if (bIsItem || bIsMimic)
		{
			// 1. 벽 체크 (플레이어와 아이템 사이 시야 확인)
			if (TraceCharacterToTarget(Actor) == false) continue;

			// 2. 대화 컴포넌트로 전달 (이미 대화한 아이템 히스토리 처리는 PetTalkComp 내부에서 수행됨)
			if (PetTalkComp)
			{
				PetTalkComp->Travel_ItemDetect(Actor, ItemDetectPingSpawnPoint->GetComponentLocation());
			}
		}
	}
}

void ABaseFlyingPet::TriggerPetBigConversation_Implementation(FName DialogueID)
{
	StartBigConversation(DialogueID);
}

void ABaseFlyingPet::TriggerPetSmallConversation_Implementation(FName DialogueID)
{
	StartSmallConversation(DialogueID);
}

void ABaseFlyingPet::SetPetState_Implementation(EPetState NewState)
{
	PetState = NewState;
}

void ABaseFlyingPet::PlayPetMontageFromConversation_Implementation(UAnimMontage* MontageToPlay)
{
	// MeshComp와 몽타주가 유효한지 확인
	if (MeshComp && MontageToPlay)
	{
		// 스켈레톤 일치 여부 확인
		// 몽타주가 사용하는 스켈레톤과 현재 펫 메쉬의 스켈레톤이 다르면 재생하지 않도록 방어 코드 추가
		USkeletalMesh* CurrentMeshAsset = MeshComp->GetSkeletalMeshAsset();
		if (CurrentMeshAsset)
		{
			USkeleton* MeshSkeleton = CurrentMeshAsset->GetSkeleton();
			USkeleton* MontageSkeleton = MontageToPlay->GetSkeleton();

			if (MeshSkeleton != MontageSkeleton)
			{
				// 스켈레톤이 다르면 경고 로그를 남기고 함수 종료
				UE_LOG(LogTemp, Error, TEXT(" NotMatch Skeleton - Montage cannot be played on this pet. "));
				return;
			}
		}
		
		// pawn은 애님 인스턴스를 직접 가져와서 재생해야 함
		UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Play(MontageToPlay);
		}
	}
}

void ABaseFlyingPet::StartBigConversation( FName DialogueID )
{
	// 2. 대화 컴포넌트에 실제 대화 시작 요청
	if (PetTalkComp)
	{
		PetTalkComp->ResetConversationLogScrollBox();
		PetTalkComp->StartConversation(DialogueID);
	}
}

void ABaseFlyingPet::EndConversation()
{
	// 따라다니기 상태로 복귀
	PetState = EPetState::EPS_Follow;
}

void ABaseFlyingPet::StartSmallConversation(FName DialogueID)
{
	if (PetTalkComp)
	{
		PetTalkComp->Travel_StartSmallConversation(DialogueID);
	}
}

#if WITH_EDITOR
void ABaseFlyingPet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// 디버그 모드에 따라 어택, 디텍트, 체이스 범위 구체의 가시성을 설정합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseFlyingPet, bDebugMode))
	{
		if ( bDebugMode == true )
		{
			if ( EnemyDetectSphere ) EnemyDetectSphere->SetVisibility(true);
			if ( ItemDetectSphere ) ItemDetectSphere->SetVisibility(true);
		}
		else
		{
			if ( EnemyDetectSphere ) EnemyDetectSphere->SetVisibility(false);
			if ( ItemDetectSphere ) ItemDetectSphere->SetVisibility(false);
		}
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABaseFlyingPet, EnemyDetectRange))
	{
		if (EnemyDetectSphere)
		{
			EnemyDetectSphere->SetSphereRadius(EnemyDetectRange);
		}
	}
	if ( PropertyName == GET_MEMBER_NAME_CHECKED(ABaseFlyingPet, ItemDetectRange) )
	{
		if ( ItemDetectSphere )
		{
			ItemDetectSphere->SetSphereRadius(ItemDetectRange);
		}
	}
}
#endif
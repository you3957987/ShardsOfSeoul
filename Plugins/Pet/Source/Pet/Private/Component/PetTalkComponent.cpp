#include "Component/PetTalkComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "FlyingPet/CuteWhalePet.h"
#include "HUD/ConversationSubtitle.h"
#include "HUD/TravelSubtitle.h"
#include "Kismet/GameplayStatics.h"
#include "Ping/PingActor.h"
#include "Header/PetState.h"
#include "Header/PetType.h"
#include "HUD/ConversationLog.h"
#include "HUD/DialogueEntry.h"
#include "Interface/PetConversationInterface.h"

UPetTalkComponent::UPetTalkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
}

void UPetTalkComponent::BeginPlay()
{
	Super::BeginPlay();

	if ( TravelSubtitleClass )
	{
		TravelSubtitleInstance = CreateWidget<UTravelSubtitle>(GetWorld(), TravelSubtitleClass);
		if (TravelSubtitleInstance)
		{
			TravelSubtitleInstance->AddToViewport();
			// 처음엔 숨기기
			TravelSubtitleInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	if ( ConversationSubtitleClass )
	{
		ConversationSubtitleInstance = CreateWidget<UConversationSubtitle>(GetWorld(), ConversationSubtitleClass);
		if (ConversationSubtitleInstance)
		{
			ConversationSubtitleInstance->AddToViewport(10); // 최상위 레이어
			// 처음엔 숨기기
			ConversationSubtitleInstance->SetVisibility(ESlateVisibility::Hidden);
			
			ConversationSubtitleInstance->OnSkipClicked.RemoveDynamic(this, &UPetTalkComponent::EndConversation);
			ConversationSubtitleInstance->OnSkipClicked.AddDynamic(this, &UPetTalkComponent::EndConversation);
			
			ConversationSubtitleInstance->OnConversationButtonClicked.RemoveDynamic(this, &UPetTalkComponent::SkipCurrentDialogue);
			ConversationSubtitleInstance->OnConversationButtonClicked.AddDynamic(this, &UPetTalkComponent::SkipCurrentDialogue);

			ConversationSubtitleInstance->OnLogClicked.RemoveDynamic(this, &UPetTalkComponent::OnPressedLogButton);
			ConversationSubtitleInstance->OnLogClicked.AddDynamic(this, &UPetTalkComponent::OnPressedLogButton);
			
			if (ConversationSubtitleInstance->LogWidgetInstance)
			{
				// 기존 연결 제거 (중복 방지)
				ConversationSubtitleInstance->LogWidgetInstance->OnCloseClicked.RemoveDynamic(this, &UPetTalkComponent::RestartConversationTimerHandle);
        
				// [수정된 부분] 델리게이트에 타이머 재시작 함수 연결
				ConversationSubtitleInstance->LogWidgetInstance->OnCloseClicked.AddDynamic(this, &UPetTalkComponent::RestartConversationTimerHandle);
			}
			
			ConversationSubtitleInstance->OnDialogueChoice_0Clicked.RemoveDynamic(this, &UPetTalkComponent::OnDialogueChoiceSelect_0);
			ConversationSubtitleInstance->OnDialogueChoice_0Clicked.AddDynamic(this, &UPetTalkComponent::OnDialogueChoiceSelect_0);
			
			ConversationSubtitleInstance->OnDialogueChoice_1Clicked.RemoveDynamic(this, &UPetTalkComponent::OnDialogueChoiceSelect_1);
			ConversationSubtitleInstance->OnDialogueChoice_1Clicked.AddDynamic(this, &UPetTalkComponent::OnDialogueChoiceSelect_1);
		}
	}
	BindInputForPet();
}

void UPetTalkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
}

void UPetTalkComponent::BindInputForPet()
{
	// [추가] 입력 액션 바인딩 (한 번만 연결해두면 됩니다)
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PC->InputComponent))
		{
			if (SkipDialogueAction)
			{
				// "Started" 타이밍에 SkipCurrentDialogue 함수 실행
				EnhancedInput->BindAction(SkipDialogueAction, ETriggerEvent::Started,
					this, &UPetTalkComponent::SkipCurrentDialogue);
			}
		}
	}
}

// 외부에서 아이템 발견시 호출 -> 랜덤하게 대사를 뽐아서 Travel_Say 호출
void UPetTalkComponent::Travel_ItemDetect(AActor* DetectedItem, const FVector& PingSpawnLocation)
{
	// 예외 처리 = 널 체크 + 이미 대사 출력한 아이템인지 확인
	if (!DetectedItem || DetectedItemHistory.Contains(DetectedItem)) return;

	SpawnItemPingEffectAtLocation( DetectedItem->GetActorLocation(), PingSpawnLocation );
	
	FPetConversationData SelectedDialogue;
	// 데이터 테이블 랜덤 함수를 통해 랜덤 데이터 가져오기
	if (GetRandomDialogueFromTable(TravelItemDetectDataTable, SelectedDialogue))
	{
		DetectedItemHistory.Add(DetectedItem);

		// 지속 시간 계산
		float Duration = 5.0f;
		
		Travel_Say(SelectedDialogue, Duration);

		// 히스토리 초기화 타이머
		if (!GetWorld()->GetTimerManager().IsTimerActive(HistoryResetTimerHandle))
		{
			GetWorld()->GetTimerManager().SetTimer(HistoryResetTimerHandle,
				this, &UPetTalkComponent::ResetDetectedItemHistory, 10.0f, false);
		}
	}
}

void UPetTalkComponent::SpawnItemPingEffectAtLocation(const FVector& TargetLocation, const FVector& PingSpawnLocation)
{
	if (!PingActorClass) return;

	// 시작 위치는 핑스폰 씬 컴포넌트 위치
	FVector StartLocation = PingSpawnLocation;

	// 핑 액터 스폰
	APingActor* PingActor = GetWorld()->SpawnActor<APingActor>(
		PingActorClass,
		StartLocation,
		FRotator::ZeroRotator
	);

	if (PingActor)
	{
		// 목표 지점으로 이동 시작 (PingActor에 구현된 함수 호출)
		PingActor->StartPingMovement(TargetLocation);
	}
}

void UPetTalkComponent::Travel_FollowToBattle(bool bBoss)
{
	UDataTable* TargetTable = bBoss ? Travel_BattleStartBoss : Travel_FollowToBattleDialogue;
	FPetConversationData SelectedDialogue;

	if (GetRandomDialogueFromTable(TargetTable, SelectedDialogue))
	{
		// 보이스 오디오가 있으면 그 길이를, 없으면 기본 3초 사용
		float Duration = (SelectedDialogue.VoiceAudio) ? SelectedDialogue.VoiceAudio->GetDuration() : 3.0f;
		if (Duration <= 0.0f) Duration = 3.0f;

		Travel_Say(SelectedDialogue, Duration);
	}
}

void UPetTalkComponent::Travel_BattleToFollow( bool bBoss )
{
	UDataTable* TargetTable = bBoss ? Travel_BattleEndBoss : Travel_BattleToFollowDialogue;
	FPetConversationData SelectedDialogue;

	if (GetRandomDialogueFromTable(TargetTable, SelectedDialogue))
	{
		// 보이스 오디오가 있으면 그 길이를, 없으면 기본 3초 사용
		float Duration = (SelectedDialogue.VoiceAudio) ? SelectedDialogue.VoiceAudio->GetDuration() : 3.0f;
		if (Duration <= 0.0f) Duration = 3.0f;

		Travel_Say(SelectedDialogue, Duration);
	}
}

void UPetTalkComponent::ResetDetectedItemHistory()
{
	DetectedItemHistory.Empty();
}

// 탐험시 화면 좌하단에 대사 출력
void UPetTalkComponent::Travel_Say(FPetConversationData DialogueData, float Duration)
{
	// 1. 현재 음성이 재생 중이라면 새로운 탐험 대사(아이템 감지, 전투 전환 등)는 무시
	if (CurrentFollowVoiceAudioComponent && CurrentFollowVoiceAudioComponent->IsPlaying()) return;
	
	// 2. 자막 표시
	if (TravelSubtitleInstance)
	{
		TravelSubtitleInstance->ShowSubtitle(DialogueData.DialogueText, Duration, DialogueData.PetIcon);
	}
	
	// 3. 음성 재생
	if (DialogueData.VoiceAudio)
	{
		// SpawnSound2D를 사용해야 IsPlaying()으로 체크가 가능합니다.
		CurrentFollowVoiceAudioComponent = UGameplayStatics::SpawnSound2D(GetWorld(), DialogueData.VoiceAudio);
	}
}

// 탐험시 특수 대사 이외의 좌하단 대화
void UPetTalkComponent::Travel_StartSmallConversation(FName DialogueID)
{
    // 1. 데이터 테이블 유효성 체크
    UDataTable* TargetTable = TravelSmallConversationDataTable ? TravelSmallConversationDataTable : BigConversationDataTable;
    if (!TargetTable || DialogueID.IsNone() || DialogueID == FName("0")) return;
    
    static const FString ContextString(TEXT("TravelSmallConversation_Context"));
    FPetConversationData* RowData = TargetTable->FindRow<FPetConversationData>(DialogueID, ContextString);

    if (RowData)
    {
        //  연속 대화를 위해 기존 음성이 있다면 중지시키고 진행
        // 이렇게 해야 IsPlaying() 가드에 걸리지 않고 다음 대사가 재생됩니다.
        if (CurrentFollowVoiceAudioComponent && CurrentFollowVoiceAudioComponent->IsPlaying())
        {
            CurrentFollowVoiceAudioComponent->Stop();
        }

        // 2. 지속 시간 계산 (보이스 우선, 없으면 5초)
        float Duration = 5.0f;
        if (RowData->VoiceAudio)
        {
            Duration = RowData->VoiceAudio->GetDuration();
        }
        if (Duration <= 0.0f) Duration = 5.0f;

        // 3. 자막 표시 및 음성 재생 (준혁님이 요청하신 직접 구현 방식)
        if (TravelSubtitleInstance)
        {
            TravelSubtitleInstance->ShowSubtitle(RowData->DialogueText, Duration, RowData->PetIcon);
        }

        if (RowData->VoiceAudio)
        {
            CurrentFollowVoiceAudioComponent = UGameplayStatics::SpawnSound2D(GetWorld(), RowData->VoiceAudio);
        }

        // 4. 다음 대사 예약 (체이닝)
        if (!RowData->NextDialogueID.IsNone() && RowData->NextDialogueID != FName("0"))
        {
            FTimerDelegate TimerDel;
            TimerDel.BindUObject(this, &UPetTalkComponent::Travel_StartSmallConversation, RowData->NextDialogueID);

            // [수정 포인트 2] 대사 사이에 아주 짧은 간격(0.1~0.2초)을 주면 훨씬 자연스럽습니다.
            float DelayBetweenLines = Duration + 0.1f; 

            GetWorld()->GetTimerManager().ClearTimer(TravelSmallConversationTimerHandle);
            GetWorld()->GetTimerManager().SetTimer(TravelSmallConversationTimerHandle, TimerDel, DelayBetweenLines, false);
        }
    }
}

// 화면 하단에 나오는 큰 대화
void UPetTalkComponent::StartConversation(FName DialogueID)
{
    //  데이터 테이블 유효성 체크
    if (!BigConversationDataTable || DialogueID.IsNone())
    {
        EndConversation(); // ID가 없으면 종료
        return;
    }

	// 인풋 게임 모드를 game and UI로 전환
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		PC->SetShowMouseCursor(true);
		PC->SetInputMode(  FInputModeGameAndUI());
	}
	
	// [컨텍스트 교체] 기본 -> 대화용
	if (!bIsInputBound)
	{
		bIsInputBound = true; // [Tick 활성화

		// [컨텍스트 교체] 기본 -> 대화용
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			PC->SetIgnoreMoveInput(true);

			if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
					LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
				{
                    
					// 2. 대화용 컨텍스트를 높은 우선순위(10)로 추가
					// 주의: 이 컨텍스트가 비어있으면 스페이스바 입력이 기본 컨텍스트로 넘어가서 점프가 발생함
					if (ConversationMappingContext)
					{
						Subsystem->AddMappingContext(ConversationMappingContext, 10);
					}
				}
			}
		}
	}

    //  데이터 테이블에서 ID로 행(Row) 검색
    static const FString ContextString(TEXT("StartConversation_Context"));
    FPetConversationData* RowData = BigConversationDataTable->FindRow<FPetConversationData>(DialogueID, ContextString);
	
    if (RowData)
    {
    	AActor* OwnerActor = GetOwner();

    	if ( OwnerActor && OwnerActor->Implements<UPetConversationInterface>() )
    	{
    		if ( RowData->bIsTalkToOtherCharacter == true ) // 딴놈이랑 대화시에는 그냥 계속 따라다니기 모드
    		{
    			IPetConversationInterface::Execute_SetPetState(OwnerActor, EPetState::EPS_Follow);
    		}
    		else // 펫이 대화시에는 대화 모드로 전환
    		{
    			IPetConversationInterface::Execute_SetPetState(OwnerActor, EPetState::EPS_Conversation);
    		}

    		if ( RowData->MontageToPlay )
    		{
    			IPetConversationInterface::Execute_PlayPetMontageFromConversation(OwnerActor, RowData->MontageToPlay);
    		}

    		// 귀여운 고래 펫일 경우
			if ( IPetConversationInterface::Execute_GetMyPetType(OwnerActor) == EPetType::EPT_CuteWhale )
			{
				ACuteWhalePet* CuteWhalePet = Cast<ACuteWhalePet>(OwnerActor);
				if ( CuteWhalePet && RowData->CuteWhale_ColorIndex != -1 && RowData->CuteWhale_FaceIndex != -1 )
				{
					CuteWhalePet->SetPetAppearance(
						RowData->CuteWhale_ColorIndex,
						RowData->CuteWhale_FaceIndex
						);
				}
			}
    	}
        //  UI 업데이트
        if (ConversationSubtitleInstance)
        {
            // 대화창이 꺼져있다면 페이드 인 (연속 대화 중에는 깜빡이지 않게 처리)
            if (!ConversationSubtitleInstance->IsVisible())
            {
                ConversationSubtitleInstance->PlayFadeInAnimation();
            }
        	
            // 텍스트 갱신
            ConversationSubtitleInstance->SetConversationSubtitle(RowData->SpeakerName, RowData->DialogueText);

        	// 선택지 모드인지 체크
        	if (RowData->bUseDialogueChoices)
        	{
        		// 위젯을 선택지 모드로 전환 (위젯 스위처 1번) 및 텍스트 설정 함수 호출 필요
        		ConversationSubtitleInstance->SetupChoiceDialogueText(RowData->Choice1_Text, RowData->Choice2_Text);
        		
        		if (ConversationSubtitleInstance->WidgetSwitcher)
        		{
        			ConversationSubtitleInstance->WidgetSwitcher->SetActiveWidgetIndex(1); // 선택지 화면으로 전환
        		}
        		
        		if (ConversationSubtitleInstance->ConversationButton)
        		{
        			// 마우스 클릭을 통과시켜 뒤의 버튼이 눌리게 함
        			ConversationSubtitleInstance->ConversationButton->SetVisibility(ESlateVisibility::Collapsed);
        		}
        		
        		// 대화 로그에 추가할 정보 저장
        		SpeakerName_Text = RowData->SpeakerName;
        		PendingChoice1_Text = RowData->Choice1_Text;
        		PendingChoice2_Text = RowData->Choice2_Text;
        		//  버튼 클릭 시 이동할 ID 저장
        		PendingChoice1_ID = RowData->Choice1_NextID;
        		PendingChoice2_ID = RowData->Choice2_NextID;

        		//  중요: 대화 타이머(ConversationTimerHandle)를 실행하지 않음 (유저 입력 대기)
        		GetWorld()->GetTimerManager().ClearTimer(ConversationTimerHandle);
        		
        		return; 
        	}
        	else
        	{
        		// [일반 대화] 위젯 스위처 0번 (기본 대화창)
        		if (ConversationSubtitleInstance->WidgetSwitcher)
        		{
        			ConversationSubtitleInstance->WidgetSwitcher->SetActiveWidgetIndex(0);
        		}
        		if (ConversationSubtitleInstance->ConversationButton)
				{
					// 선택지 모드가 아니면 대화창 전체를 클릭 가능하게 설정
					ConversationSubtitleInstance->ConversationButton->SetVisibility(ESlateVisibility::Visible);
				}
        	}
        	
        	// 대화 로그에 추가
			AddDialogueToConversationLog(RowData->SpeakerName, RowData->DialogueText);
        }

    	//  음성 재생 및 지속 시간 계산
    	float Duration = 10.0f;
    	if (RowData->VoiceAudio)
    	{
    		//  기존 음성 중지 로직 추가
    		if (CurrentConversationVoiceAudioComponent && CurrentConversationVoiceAudioComponent->IsPlaying())
    		{
    			CurrentConversationVoiceAudioComponent->Stop();
    		}

    		// [수정] SpawnSound2D로 변경하여 제어권 획득
    		CurrentConversationVoiceAudioComponent = UGameplayStatics::SpawnSound2D(GetWorld(), RowData->VoiceAudio);
			
    		Duration = RowData->VoiceAudio->GetDuration();
    	}
        // 안전 장치: 너무 짧으면 7초로 고정
        if (Duration <= 10.0f) Duration = 10.0f;

        //  다음 대사 예약 (체이닝 로직)
    	
    	NextDialogueID = RowData->NextDialogueID;
    	
        FTimerDelegate TimerDel;
        if (!NextDialogueID.IsNone() && NextDialogueID != FName("0")) // 다음 대화 ID가 있으면
        {
            // 다음 대화 ID가 있으면: Duration 후에 StartConversation을 다시 호출 (재귀)
            TimerDel.BindUObject(this, &UPetTalkComponent::StartConversation, NextDialogueID);
        }
        else
        {
            // 다음 대화 ID가 없으면(None): Duration 후에 대화 종료
            TimerDel.BindUObject(this, &UPetTalkComponent::EndConversation);
        }

        // 기존 타이머 초기화 후 새로 설정
        GetWorld()->GetTimerManager().ClearTimer(ConversationTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(ConversationTimerHandle, TimerDel, Duration, false);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Dialogue ID '%s' not found in DataTable."), *DialogueID.ToString());
        EndConversation();
    }
}

void UPetTalkComponent::SkipCurrentDialogue()
{
	// 대화 타이머가 돌고 있지 않다면(대화 중이 아님) 무시
	if (!GetWorld()->GetTimerManager().IsTimerActive(ConversationTimerHandle))
	{
		return;
	}

	//  현재 진행 중인 타이머 강제 종료
	GetWorld()->GetTimerManager().ClearTimer(ConversationTimerHandle);

	//  현재 재생 중인 음성 즉시 중지
	if (CurrentConversationVoiceAudioComponent && CurrentConversationVoiceAudioComponent->IsPlaying())
	{
		CurrentConversationVoiceAudioComponent->Stop();
	}

	//  즉시 다음 로직 실행
	// 다음 대화가 있으면 바로 시작, 없으면 종료
	if (!NextDialogueID.IsNone() && NextDialogueID != FName("0"))
	{
		StartConversation(NextDialogueID);
	}
	else
	{
		EndConversation();
	}
}

void UPetTalkComponent::EndConversation()
{
	//  UI 페이드 아웃 처리
	if (ConversationSubtitleInstance)
	{
		ConversationSubtitleInstance->PlayFadeOutAnimation();
	}

	// [추가] 대화 상태 플래그 해제 (Tick의 스킵 로직 중지)
	bIsInputBound = false;

	// [컨텍스트 복구] 대화용 -> 기본
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				// 1. 대화용 컨텍스트 제거
				if (ConversationMappingContext)
				{
					Subsystem->RemoveMappingContext(ConversationMappingContext);
				}
				
			}
		}
		// [추가] 마우스 커서 숨기기 및 인풋 모드 복구 (GameOnly)
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
		
		// 이동 입력 차단 해제
		PC->SetIgnoreMoveInput(false);
	}

	GetWorld()->GetTimerManager().ClearTimer(ConversationTimerHandle); // 대화 타이머 초기화

	//  현재 재생 중인 음성 즉시 중지
	if (CurrentConversationVoiceAudioComponent && CurrentConversationVoiceAudioComponent->IsPlaying())
	{
		CurrentConversationVoiceAudioComponent->Stop();
	}
	
	AActor* OwnerActor = GetOwner();
	
	// 귀여운 고래 펫일 경우
	if ( IPetConversationInterface::Execute_GetMyPetType(OwnerActor) == EPetType::EPT_CuteWhale )
	{
		ACuteWhalePet* CuteWhalePet = Cast<ACuteWhalePet>(OwnerActor);
		if ( CuteWhalePet )
		{
			CuteWhalePet->SetPetAppearance(0,0); // 디폴트 색상 및 표정으로 복귀
		}
	}
	
	// 이 컴포넌트를 가진 액터가 이 이벤트를 (Bind)하면 실행됨
	if (OnConversationEnded.IsBound())
	{
		OnConversationEnded.Broadcast();
	}
}

// 데이터 테이블에서 랜덤한 행 데이터 OutData 반환
bool UPetTalkComponent::GetRandomDialogueFromTable(UDataTable* DataTable, FPetConversationData& OutData)
{
	if (!DataTable) return false;

	TArray<FName> RowNames = DataTable->GetRowNames();
	if (RowNames.Num() == 0) return false;

	int32 RandomIndex = FMath::RandRange(0, RowNames.Num() - 1);
	FName SelectedRowName = RowNames[RandomIndex];

	static const FString ContextString(TEXT("GetRandomDialogue_Context"));
	FPetConversationData* RowData = DataTable->FindRow<FPetConversationData>(SelectedRowName, ContextString);

	if (RowData)
	{
		OutData = *RowData; // 구조체 복사
		return true;
	}
	return false;
}

void UPetTalkComponent::ResetConversationLogScrollBox()
{
	if (ConversationSubtitleInstance)
	{
		if ( ConversationSubtitleInstance->LogWidgetInstance )
		{
			ConversationSubtitleInstance->LogWidgetInstance->ScrollBox->ClearChildren();
		}
	}
}

void UPetTalkComponent::OnPressedLogButton()
{
	if (ConversationSubtitleInstance)
	{
		if ( ConversationSubtitleInstance->LogWidgetInstance )
		{
			ConversationSubtitleInstance->LogWidgetInstance->SetVisibility( ESlateVisibility::Visible );

			// 대화 일시정지시켜서 다음으로 못 넘어가도록
			GetWorld()->GetTimerManager().PauseTimer(ConversationTimerHandle);
		}
	}
}

void UPetTalkComponent::RestartConversationTimerHandle()
{
	GetWorld()->GetTimerManager().UnPauseTimer(ConversationTimerHandle);
}

void UPetTalkComponent::AddDialogueToConversationLog(const FText& SpeakerName, const FText& DialogueText, bool bIsSelection)
{
	// 1. 유효성 검사 (서브타이틀 -> 로그창 -> 스크롤박스까지 연결 확인)
	if (!ConversationSubtitleInstance ||
		!ConversationSubtitleInstance->LogWidgetInstance ||
		!ConversationSubtitleInstance->LogWidgetInstance->ScrollBox)
	{
		return;
	}
	// 2. 개별 로그 위젯 클래스가 설정되어 있는지 확인
	if (!ConversationLogEntryClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConversationLogEntryClass is not set in PetTalkComponent."));
		return;
	}

	// 3. 개별 로그 위젯 생성
	UDialogueEntry* NewLogEntry = CreateWidget<UDialogueEntry>(GetWorld(), ConversationLogEntryClass);

	if (NewLogEntry)
	{
		// 4. 데이터 설정 (UConversationLog 안에 해당 함수 구현 필요)
		// 예: 스피커 이름과 대사 내용 전달
		if ( bIsSelection == true ) NewLogEntry->SetLogData(SpeakerName, DialogueText, true);
		else if ( bIsSelection == false ) NewLogEntry->SetLogData(SpeakerName, DialogueText, false);
		
		// 5. 스크롤 박스에 자식으로 추가
		ConversationSubtitleInstance->LogWidgetInstance->ScrollBox->AddChild(NewLogEntry);

		// 6. 스크롤을 항상 최신 내용(맨 아래)으로 이동
		ConversationSubtitleInstance->LogWidgetInstance->ScrollBox->ScrollToEnd();
	}
}

void UPetTalkComponent::OnDialogueChoiceSelect_0()
{
	AddDialogueToConversationLog(SpeakerName_Text, PendingChoice1_Text, true);

	// 저장해둔 1번 선택지 ID로 대화 진행
	if (!PendingChoice1_ID.IsNone())
	{
		StartConversation(PendingChoice1_ID);
	}
	else
	{
		EndConversation();
	}
}

void UPetTalkComponent::OnDialogueChoiceSelect_1()
{
	AddDialogueToConversationLog(SpeakerName_Text, PendingChoice2_Text, true);

	// 저장해둔 2번 선택지 ID로 대화 진행
	if (!PendingChoice2_ID.IsNone())
	{
		StartConversation(PendingChoice2_ID);
	}
	else
	{
		EndConversation();
	}
}

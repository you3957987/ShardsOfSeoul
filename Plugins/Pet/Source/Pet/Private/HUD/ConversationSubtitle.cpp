#include "HUD/ConversationSubtitle.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "HUD/ConversationLog.h"

void UConversationSubtitle::NativeConstruct()
{
	Super::NativeConstruct();

	if (SkipButton) SkipButton->OnClicked.AddDynamic(this, &UConversationSubtitle::OnPressedSkipButton);
	if (LogButton) LogButton->OnClicked.AddDynamic(this, &UConversationSubtitle::OnPressedLogButton);
	if (ConversationButton) ConversationButton->OnClicked.AddDynamic(this, &UConversationSubtitle::OnPressedConversationButton);
	
	if (ConversationLogWidgetClass && !LogWidgetInstance)
	{
		LogWidgetInstance = CreateWidget<UConversationLog>(GetWorld(), ConversationLogWidgetClass);

		if (LogWidgetInstance)
		{
			// 뷰포트에 추가는 해두지만, 당장은 안 보이게 숨김
			LogWidgetInstance->AddToViewport(20); // ZOrder를 높게 설정하여 대화창보다 위에 뜨게 함 (선택사항)
			LogWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if(DialogueChoiceButton_0)
	{
		DialogueChoiceButton_0->OnClicked.AddDynamic(this, &UConversationSubtitle::OnClickDialogueChoice_0);
	}
	if(DialogueChoiceButton_1)
	{
		DialogueChoiceButton_1->OnClicked.AddDynamic(this, &UConversationSubtitle::OnClickDialogueChoice_1);
	}
}

void UConversationSubtitle::SetConversationSubtitle(const FText& InName, const FText& InDialogue)
{
	if (Text_Name)
	{
		// [수정] 이미 FText이므로 FromString 없이 바로 넣습니다.
		Text_Name->SetText(InName);
	}

	if (Text_Dialogue)
	{
		Text_Dialogue->SetText(InDialogue);
	}
}

void UConversationSubtitle::PlayFadeInAnimation()
{
	//보이게 설정
	SetVisibility(ESlateVisibility::Visible);
	
	if (FadeInAnim)
	{
		PlayAnimation(FadeInAnim);
	}
}

void UConversationSubtitle::PlayFadeOutAnimation()
{
	if (FadeOutAnim)
	{
		PlayAnimation(FadeOutAnim);

		// FadeOut 애니메이션 길이만큼 기다렸다가 완전히 숨김 (예: 0.5초)
		float AnimEndTime = FadeOutAnim->GetEndTime();
		GetWorld()->GetTimerManager().SetTimer(FadeOutTimerHandle, this, &UConversationSubtitle::OnFadeOutFinished,
			AnimEndTime, false);
	}
	else
	{
		// 애니메이션 없으면 바로 숨김
		OnFadeOutFinished();
	}
}

void UConversationSubtitle::OnFadeOutFinished()
{
	SetVisibility(ESlateVisibility::Hidden);
}

void UConversationSubtitle::OnPressedSkipButton()
{
	//UE_LOG(LogTemp, Warning, TEXT("OnPressedSkipButton"));

	if (OnSkipClicked.IsBound())
	{
		OnSkipClicked.Broadcast(); 
	}
}

void UConversationSubtitle::OnPressedLogButton()
{
	//UE_LOG(LogTemp, Warning, TEXT("OnPressedLogButton"));

	if (OnLogClicked.IsBound())
	{
		OnLogClicked.Broadcast();
	}
}

void UConversationSubtitle::OnPressedConversationButton()
{
	if (OnConversationButtonClicked.IsBound())
	{
		OnConversationButtonClicked.Broadcast();
	}
}

void UConversationSubtitle::SetupChoiceDialogueText(const FText& Choice0Text, const FText& Choice1Text)
{
	if ( DialogueChoiceText_0 && DialogueChoiceText_1 )
	{
		DialogueChoiceText_0->SetText( Choice0Text );
		DialogueChoiceText_1->SetText( Choice1Text );
	}
}

void UConversationSubtitle::OnClickDialogueChoice_0()
{
	if (OnDialogueChoice_0Clicked.IsBound())
	{
		OnDialogueChoice_0Clicked.Broadcast();
	}
}

void UConversationSubtitle::OnClickDialogueChoice_1()
{
	if (OnDialogueChoice_1Clicked.IsBound())
	{
		OnDialogueChoice_1Clicked.Broadcast();
	}
}

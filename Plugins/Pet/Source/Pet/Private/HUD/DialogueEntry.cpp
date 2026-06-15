#include "HUD/DialogueEntry.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UDialogueEntry::SetLogData(const FText& InSpeakerName, const FText& InDialogueText, bool bIsSelection)
{
	// 1. 스피커 이름 설정
	if (SpeakerName)
	{
		SpeakerName->SetText(InSpeakerName);
	}
	// 2. 대화 내용 설정
	if (DialogueText)
	{
		DialogueText->SetText(InDialogueText);
		if (bIsSelection == true)
		{
			DialogueText->SetColorAndOpacity(FSlateColor(
				FLinearColor::FromSRGBColor(FColor::FromHex("#FFD700"))));

			TextBorder->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)); // 테두리 활성화

			// 배경색 활성화
			TextBackColor->SetVisibility( ESlateVisibility::Visible );
		}
	}
}

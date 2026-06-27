#include "CSVLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

void UCSVLog::AddEnemyLog(const FString& LogCategory, const FString& EnemyID, const FString& EnemyType, const FEnemyLogData& LogData)
{
	// 1. 저장 경로 설정 (Saved/CSVLogs/[LogCategory]/[EnemyLogID].csv)
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CSVLogs") / LogCategory;
	const FString FileName = TEXT("EnemyLog.csv");
	const FString FilePath = Directory / FileName;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 2. 디렉토리가 없으면 자동 생성
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	// 3. 파일 존재 여부 확인 (헤더 추가 여부 결정)
	bool bFileExists = PlatformFile.FileExists(*FilePath);
	FString FinalOutputString = TEXT("");

	// 4. 새로운 파일이라면 기획한 헤더(제목줄) 먼저 기록
	if (!bFileExists)
	{
		FinalOutputString += FEnemyLogData::GetCSVHeader() + TEXT("\n");
	}

	// 5. 데이터 포팅 및 직렬화
	// 구조체 내부의 ID와 Type이 비어있다면, 필수 인자로 들어온 값을 채워줍니다. (데이터 백업 로직)
	FEnemyLogData FinalData = LogData;
	
	if (FinalData.EnemyID.IsEmpty()) FinalData.EnemyID = EnemyID;
	if (FinalData.EnemyType.IsEmpty()) FinalData.EnemyType = EnemyType;

	// 6. 한 줄의 가로 행(Row)으로 변환하여 추가
	FinalOutputString += FinalData.ToCSVRow() + TEXT("\n");

	// 7. 파일의 끝에 내용 추가 (Append 모드)
	FFileHelper::SaveStringToFile(
		FinalOutputString,
		*FilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, // 한글 깨짐 방지
		&IFileManager::Get(),
		FILEWRITE_Append
	);
}

void UCSVLog::AddSkeletonMageLog(const FString& LogCategory, const FBossSkeletonMageLogData& LogData)
{
	// 저장 경로 설정 (Saved/CSVLogs/[LogCategory]/SkeletonMageLog.csv)
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CSVLogs") / LogCategory;
	const FString FileName = TEXT("SkeletonMageLog.csv");
	const FString FilePath = Directory / FileName;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 디렉토리 자동 생성
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	bool bFileExists = PlatformFile.FileExists(*FilePath);
	FString FinalOutputString = TEXT("");

	// 새 파일이면 헤더 추가
	if (!bFileExists)
	{
		FinalOutputString += FBossSkeletonMageLogData::GetCSVHeader() + TEXT("\n");
	}

	// 데이터 백업 및 예외 처리
	FBossSkeletonMageLogData FinalData = LogData;
	if (FinalData.Base.BossID.IsEmpty())
	{
		FinalData.Base.BossID = TEXT("SkeletonMage_Unknown");
	}

	// CSV 행 변환 및 누적
	FinalOutputString += FinalData.ToCSVRow() + TEXT("\n");

	// 파일에 추가 저장
	FFileHelper::SaveStringToFile(
		FinalOutputString,
		*FilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(),
		FILEWRITE_Append
	);
}

void UCSVLog::AddMagicSwordManLog(const FString& LogCategory, const FBossMagicSwordManLogData& LogData)
{
	// 저장 경로 설정 (Saved/CSVLogs/[LogCategory]/MagicSwordMan.csv)
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CSVLogs") / LogCategory;
	const FString FileName = TEXT("MagicSwordMan.csv");
	const FString FilePath = Directory / FileName;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 디렉토리 자동 생성
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	bool bFileExists = PlatformFile.FileExists(*FilePath);
	FString FinalOutputString = TEXT("");

	// 새 파일이면 헤더 추가
	if (!bFileExists)
	{
		FinalOutputString += FBossMagicSwordManLogData::GetCSVHeader() + TEXT("\n");
	}

	// 데이터 백업 및 예외 처리
	FBossMagicSwordManLogData FinalData = LogData;
	if (FinalData.Base.BossID.IsEmpty())
	{
		FinalData.Base.BossID = TEXT("FBossMagicSwordManLogData_Unknown");
	}

	// CSV 행 변환 및 누적
	FinalOutputString += FinalData.ToCSVRow() + TEXT("\n");

	// 파일에 추가 저장
	FFileHelper::SaveStringToFile(
		FinalOutputString,
		*FilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(),
		FILEWRITE_Append
	);
}

void UCSVLog::AddHechiLog(const FString& LogCategory, const FHechiLogData& LogData)
{
	// 저장 경로 설정 (Saved/CSVLogs/[LogCategory]/MagicSwordMan.csv)
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CSVLogs") / LogCategory;
	const FString FileName = TEXT("Hechi.csv");
	const FString FilePath = Directory / FileName;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// 디렉토리 자동 생성
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	bool bFileExists = PlatformFile.FileExists(*FilePath);
	FString FinalOutputString = TEXT("");

	// 새 파일이면 헤더 추가
	if (!bFileExists)
	{
		FinalOutputString += FHechiLogData::GetCSVHeader() + TEXT("\n");
	}

	// 데이터 백업 및 예외 처리
	FHechiLogData FinalData = LogData;
	if (FinalData.Base.BossID.IsEmpty())
	{
		FinalData.Base.BossID = TEXT("FHechiLogData_Unknown");
	}

	// CSV 행 변환 및 누적
	FinalOutputString += FinalData.ToCSVRow() + TEXT("\n");

	// 파일에 추가 저장
	FFileHelper::SaveStringToFile(
		FinalOutputString,
		*FilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(),
		FILEWRITE_Append
	);
}

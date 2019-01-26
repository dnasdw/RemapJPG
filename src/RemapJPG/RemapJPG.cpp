#include <sdw.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

struct SJpgRecord
{
	vector<UString> OldFileName;
	UString NewFileName;
	u32 CRC32;
	u32 Width;
	u32 Height;
};

int getJpgFileName(const UString& a_sDirName, vector<UString>& a_vJpgFileName)
{
	queue<UString> qDir;
	qDir.push(a_sDirName);
	while (!qDir.empty())
	{
		UString& sParent = qDir.front();
#if SDW_PLATFORM == SDW_PLATFORM_WINDOWS
		WIN32_FIND_DATAW ffd;
		HANDLE hFind = INVALID_HANDLE_VALUE;
		wstring sPattern = sParent + L"/*";
		hFind = FindFirstFileW(sPattern.c_str(), &ffd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && EndWith<wstring>(ffd.cFileName, L".jpg"))
				{
					wstring sFileName = sParent + L"/" + ffd.cFileName;
					a_vJpgFileName.push_back(sFileName);
				}
				else if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && wcscmp(ffd.cFileName, L".") != 0 && wcscmp(ffd.cFileName, L"..") != 0)
				{
					wstring sDir = sParent + L"/" + ffd.cFileName;
					qDir.push(sDir);
				}
			} while (FindNextFileW(hFind, &ffd) != 0);
		}
#else
		DIR* pDir = opendir(sParent.c_str());
		if (pDir != nullptr)
		{
			dirent* pDirent = nullptr;
			while ((pDirent = readdir(pDir)) != nullptr)
			{
				if (pDirent->d_type == DT_REG && EndWith<string>(pDirent->d_name, ".jpg"))
				{
					string sFileName = sParent + "/" + pDirent->d_name;
					a_vJpgFileName.push_back(sFileName);
				}
				else if (pDirent->d_type == DT_DIR && strcmp(pDirent->d_name, ".") != 0 && strcmp(pDirent->d_name, "..") != 0)
				{
					string sDir = sParent + "/" + pDirent->d_name;
					qDir.push(sDir);
				}
			}
			closedir(pDir);
		}
#endif
		qDir.pop();
	}
	sort(a_vJpgFileName.begin(), a_vJpgFileName.end());
	return 0;
}

int resaveJpg(const UString& a_sDirName)
{
	vector<UString> vJpgFileName;
	if (getJpgFileName(a_sDirName, vJpgFileName) != 0)
	{
		return 1;
	}
	for (vector<UString>::iterator it = vJpgFileName.begin(); it != vJpgFileName.end(); ++it)
	{
		UString& sJpgFileName = *it;
		FILE* fp = UFopen(sJpgFileName.c_str(), USTR("rb"), false);
		if (fp == nullptr)
		{
			return 1;
		}
		u32 uJpgWidth = 0;
		u32 uJpgHeight = 0;
		n32 nJpgChannel = 0;
		u8* pData = stbi_load_from_file(fp, reinterpret_cast<n32*>(&uJpgWidth), reinterpret_cast<n32*>(&uJpgHeight), &nJpgChannel, 3);
		fclose(fp);
		if (pData == nullptr)
		{
			return 1;
		}
		fp = UFopen(sJpgFileName.c_str(), USTR("wb"), false);
		if (fp == nullptr)
		{
			stbi_image_free(pData);
			return 1;
		}
		stbi_write_jpg_to_func(stbi__stdio_write, fp, uJpgWidth, uJpgHeight, 3, pData, 100);
		fclose(fp);
		stbi_image_free(pData);
	}
	return 0;
}

int exportJpg(const UString& a_sOldDirName, const UString& a_sNewDirName, const UString& a_sRecordFileName)
{
	vector<UString> vJpgFileName;
	if (getJpgFileName(a_sOldDirName, vJpgFileName) != 0)
	{
		return 1;
	}
	UMkdir(a_sNewDirName.c_str());
	vector<SJpgRecord> vJpgRecord;
	for (vector<UString>::iterator it = vJpgFileName.begin(); it != vJpgFileName.end(); ++it)
	{
		UString sJpgFileName = *it;
		FILE* fp = UFopen(sJpgFileName.c_str(), USTR("rb"), false);
		if (fp == nullptr)
		{
			return 1;
		}
		u32 uJpgWidth = 0;
		u32 uJpgHeight = 0;
		n32 nJpgChannel = 0;
		u8* pData = stbi_load_from_file(fp, reinterpret_cast<n32*>(&uJpgWidth), reinterpret_cast<n32*>(&uJpgHeight), &nJpgChannel, 3);
		fclose(fp);
		if (pData == nullptr)
		{
			return 1;
		}
		u32 uCRC32 = UpdateCRC32(pData, uJpgWidth * uJpgHeight * 3);
		UString sOldFileName = sJpgFileName.substr(a_sOldDirName.size() + 1);
		UString sNewFileName = Replace(sOldFileName, USTR("/"), USTR("_"));
		bool bSame = false;
		for (vector<SJpgRecord>::iterator itRecord = vJpgRecord.begin(); itRecord != vJpgRecord.end(); ++itRecord)
		{
			SJpgRecord& jpgRecord = *itRecord;
			if (jpgRecord.CRC32 == uCRC32 && jpgRecord.Width == uJpgWidth && jpgRecord.Height == uJpgHeight)
			{
				jpgRecord.OldFileName.push_back(sOldFileName);
				bSame = true;
				break;
			}
		}
		if (!bSame)
		{
			vJpgRecord.resize(vJpgRecord.size() + 1);
			SJpgRecord& jpgRecord = vJpgRecord.back();
			jpgRecord.OldFileName.push_back(sOldFileName);
			jpgRecord.NewFileName = sNewFileName;
			jpgRecord.CRC32 = uCRC32;
			jpgRecord.Width = uJpgWidth;
			jpgRecord.Height = uJpgHeight;
			sJpgFileName = a_sNewDirName + USTR("/") + sNewFileName;
			fp = UFopen(sJpgFileName.c_str(), USTR("wb"), false);
			if (fp == nullptr)
			{
				stbi_image_free(pData);
				return 1;
			}
			stbi_write_jpg_to_func(stbi__stdio_write, fp, uJpgWidth, uJpgHeight, 3, pData, 100);
			fclose(fp);
		}
		stbi_image_free(pData);
	}
	FILE* fp = UFopen(a_sRecordFileName.c_str(), USTR("wb"), false);
	if (fp == nullptr)
	{
		return 1;
	}
	fwrite("\xFF\xFE", 2, 1, fp);
	n32 nIndex = 0;
	for (vector<SJpgRecord>::iterator itRecord = vJpgRecord.begin(); itRecord != vJpgRecord.end(); ++itRecord)
	{
		SJpgRecord& jpgRecord = *itRecord;
		if (ftell(fp) != 2)
		{
			fu16printf(fp, L"\r\n\r\n");
		}
		fu16printf(fp, L"No.%d\r\n", nIndex++);
		fu16printf(fp, L"--------------------------------------\r\n");
		fu16printf(fp, L"%ls\r\n", jpgRecord.NewFileName.c_str());
		fu16printf(fp, L"======================================\r\n");
		for (vector<UString>::iterator it = jpgRecord.OldFileName.begin(); it != jpgRecord.OldFileName.end(); ++it)
		{
			fu16printf(fp, L"%ls\r\n", it->c_str());
		}
		fu16printf(fp, L"--------------------------------------\r\n");
	}
	fclose(fp);
	return 0;
}

int importJpg(const UString& a_sOldDirName, const UString& a_sNewDirName, const UString& a_sRecordFileName)
{
	FILE* fp = UFopen(a_sRecordFileName.c_str(), USTR("rb"), false);
	if (fp == nullptr)
	{
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	u32 uTxtSize = ftell(fp);
	if (uTxtSize % 2 != 0)
	{
		fclose(fp);
		return 1;
	}
	uTxtSize /= 2;
	fseek(fp, 0, SEEK_SET);
	Char16_t* pTemp = new Char16_t[uTxtSize + 1];
	fread(pTemp, 2, uTxtSize, fp);
	fclose(fp);
	if (pTemp[0] != 0xFEFF)
	{
		delete[] pTemp;
		return 1;
	}
	pTemp[uTxtSize] = 0;
	wstring sTxt = U16ToW(pTemp + 1);
	delete[] pTemp;
	vector<SJpgRecord> vJpgRecord;
	wstring::size_type uPos0 = 0;
	while ((uPos0 = sTxt.find(L"No.", uPos0)) != wstring::npos)
	{
		wstring::size_type uPos1 = sTxt.find(L"\r\n--------------------------------------\r\n", uPos0);
		if (uPos1 == wstring::npos)
		{
			return 1;
		}
		uPos0 = uPos1 + wcslen(L"\r\n--------------------------------------\r\n");
		uPos1 = sTxt.find(L"\r\n======================================\r\n", uPos0);
		if (uPos1 == wstring::npos)
		{
			return 1;
		}
		wstring sStmtOld = sTxt.substr(uPos0, uPos1 - uPos0);
		uPos0 = uPos1 + wcslen(L"\r\n======================================\r\n");
		uPos1 = sTxt.find(L"\r\n--------------------------------------", uPos0);
		if (uPos1 == wstring::npos)
		{
			return 1;
		}
		wstring sStmtNew = sTxt.substr(uPos0, uPos1 - uPos0);
		uPos0 = uPos1 + wcslen(L"\r\n--------------------------------------");
		SJpgRecord jpgRecord;
		jpgRecord.OldFileName = Split(WToU(sStmtNew), USTR("\r\n"));
		jpgRecord.NewFileName = WToU(sStmtOld);
		vJpgRecord.push_back(jpgRecord);
	}
	for (vector<SJpgRecord>::iterator itRecord = vJpgRecord.begin(); itRecord != vJpgRecord.end(); ++itRecord)
	{
		SJpgRecord& jpgRecord = *itRecord;
		UString sJpgFileName = a_sNewDirName + USTR("/") + jpgRecord.NewFileName;
		fp = UFopen(sJpgFileName.c_str(), USTR("rb"), false);
		if (fp == nullptr)
		{
			return 1;
		}
		u32 uJpgWidth = 0;
		u32 uJpgHeight = 0;
		n32 nJpgChannel = 0;
		u8* pData = stbi_load_from_file(fp, reinterpret_cast<n32*>(&uJpgWidth), reinterpret_cast<n32*>(&uJpgHeight), &nJpgChannel, 3);
		fclose(fp);
		if (fp == nullptr)
		{
			return 1;
		}
		for (vector<UString>::iterator it = jpgRecord.OldFileName.begin(); it != jpgRecord.OldFileName.end(); ++it)
		{
			vector<UString> vDirPath = SplitOf(*it, USTR("/\\"));
			UString sDirName = a_sOldDirName;
			for (n32 i = 0; i < static_cast<n32>(vDirPath.size()) - 1; i++)
			{
				sDirName += USTR("/") + vDirPath[i];
				UMkdir(sDirName.c_str());
			}
			sJpgFileName = a_sOldDirName + USTR("/") + *it;
			fp = UFopen(sJpgFileName.c_str(), USTR("wb"), false);
			if (fp == nullptr)
			{
				stbi_image_free(pData);
				return 1;
			}
			stbi_write_jpg_to_func(stbi__stdio_write, fp, uJpgWidth, uJpgHeight, 3, pData, 100);
			fclose(fp);
		}
		stbi_image_free(pData);
	}
	return 0;
}

int export2Jpg(const UString& a_sOldDirName, const UString& a_sNewDirName, const UString& a_sRecordFileName)
{
	FILE* fp = UFopen(a_sRecordFileName.c_str(), USTR("rb"), false);
	if (fp == nullptr)
	{
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	u32 uTxtSize = ftell(fp);
	if (uTxtSize % 2 != 0)
	{
		fclose(fp);
		return 1;
	}
	uTxtSize /= 2;
	fseek(fp, 0, SEEK_SET);
	Char16_t* pTemp = new Char16_t[uTxtSize + 1];
	fread(pTemp, 2, uTxtSize, fp);
	fclose(fp);
	if (pTemp[0] != 0xFEFF)
	{
		delete[] pTemp;
		return 1;
	}
	pTemp[uTxtSize] = 0;
	wstring sTxt = U16ToW(pTemp + 1);
	delete[] pTemp;
	vector<SJpgRecord> vJpgRecord;
	wstring::size_type uPos0 = 0;
	while ((uPos0 = sTxt.find(L"No.", uPos0)) != wstring::npos)
	{
		wstring::size_type uPos1 = sTxt.find(L"\r\n--------------------------------------\r\n", uPos0);
		if (uPos1 == wstring::npos)
		{
			return 1;
		}
		uPos0 = uPos1 + wcslen(L"\r\n--------------------------------------\r\n");
		uPos1 = sTxt.find(L"\r\n======================================\r\n", uPos0);
		if (uPos1 == wstring::npos)
		{
			return 1;
		}
		wstring sStmtOld = sTxt.substr(uPos0, uPos1 - uPos0);
		uPos0 = uPos1 + wcslen(L"\r\n======================================\r\n");
		uPos1 = sTxt.find(L"\r\n--------------------------------------", uPos0);
		if (uPos1 == wstring::npos)
		{
			return 1;
		}
		wstring sStmtNew = sTxt.substr(uPos0, uPos1 - uPos0);
		uPos0 = uPos1 + wcslen(L"\r\n--------------------------------------");
		SJpgRecord jpgRecord;
		jpgRecord.OldFileName = Split(WToU(sStmtNew), USTR("\r\n"));
		jpgRecord.NewFileName = WToU(sStmtOld);
		vJpgRecord.push_back(jpgRecord);
	}
	UMkdir(a_sNewDirName.c_str());
	for (vector<SJpgRecord>::iterator itRecord = vJpgRecord.begin(); itRecord != vJpgRecord.end(); ++itRecord)
	{
		SJpgRecord& jpgRecord = *itRecord;
		UString sJpgFileName = a_sOldDirName + USTR("/") + jpgRecord.OldFileName[0];
		fp = UFopen(sJpgFileName.c_str(), USTR("rb"), false);
		if (fp == nullptr)
		{
			return 1;
		}
		u32 uJpgWidth = 0;
		u32 uJpgHeight = 0;
		n32 nJpgChannel = 0;
		u8* pData = stbi_load_from_file(fp, reinterpret_cast<n32*>(&uJpgWidth), reinterpret_cast<n32*>(&uJpgHeight), &nJpgChannel, 3);
		fclose(fp);
		if (pData == nullptr)
		{
			return 1;
		}
		sJpgFileName = a_sNewDirName + USTR("/") + jpgRecord.NewFileName;
		fp = UFopen(sJpgFileName.c_str(), USTR("wb"), false);
		if (fp == nullptr)
		{
			stbi_image_free(pData);
			return 1;
		}
		stbi_write_jpg_to_func(stbi__stdio_write, fp, uJpgWidth, uJpgHeight, 3, pData, 100);
		fclose(fp);
		stbi_image_free(pData);
	}
	return 0;
}

int UMain(int argc, UChar* argv[])
{
	if (argc < 3)
	{
		return 1;
	}
	if (UCslen(argv[1]) == 1)
	{
		switch (*argv[1])
		{
		case USTR('R'):
		case USTR('r'):
			if (argc != 3)
			{
				return 1;
			}
			return resaveJpg(argv[2]);
		case USTR('E'):
		case USTR('e'):
			if (argc != 5)
			{
				return 1;
			}
			return exportJpg(argv[2], argv[3], argv[4]);
		case USTR('I'):
		case USTR('i'):
			if (argc != 5)
			{
				return 1;
			}
			return importJpg(argv[2], argv[3], argv[4]);
		case USTR('M'):
		case USTR('m'):
			if (argc != 5)
			{
				return 1;
			}
			return export2Jpg(argv[2], argv[3], argv[4]);
		default:
			break;
		}
	}
	return 1;
}

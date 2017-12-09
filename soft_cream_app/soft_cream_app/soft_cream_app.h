
// soft_cream_app.h : PROJECT_NAME アプリケーションのメイン ヘッダー ファイルです。
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH に対してこのファイルをインクルードする前に 'stdafx.h' をインクルードしてください"
#endif

#include "resource.h"		// メイン シンボル


// Csoft_cream_appApp:
// このクラスの実装については、soft_cream_app.cpp を参照してください。
//

class Csoft_cream_appApp : public CWinApp
{
public:
	Csoft_cream_appApp();

// オーバーライド
public:
	virtual BOOL InitInstance();

// 実装

	DECLARE_MESSAGE_MAP()
};

extern Csoft_cream_appApp theApp;
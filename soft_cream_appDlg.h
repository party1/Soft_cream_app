
// soft_cream_appDlg.h : ヘッダー ファイル
//

#pragma once
#include "afxwin.h"


// Csoft_cream_appDlg ダイアログ
class Csoft_cream_appDlg : public CDialogEx
{
// コンストラクション
public:
	Csoft_cream_appDlg(CWnd* pParent = NULL);	// 標準コンストラクター

// ダイアログ データ
	enum { IDD = IDD_SOFT_CREAM_APP_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV サポート


// 実装
protected:
	HICON m_hIcon;

	// 生成された、メッセージ割り当て関数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDestroy();

private:
	CStatic m_glView;
	CStatic m_glView2;
	CDC *m_pDC;
	HGLRC m_GLRC;
	BOOL SetUpPixelFormat(HDC hdc);
public:
	CEdit msgED;
	LRESULT OnMessageRCV(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
//	CStatic IDC_PICT0;
	afx_msg void OnStnClickedPict0();
private:
	int m_xvRadio;
public:
	afx_msg void OnBnClickedRadio1();
	afx_msg void OnBnClickedButton3();

	//以下実験
	int hide_cone = 0;//スイッチ
	unsigned int m_nTimer;//タイマー(仮)
	afx_msg void OnTimer(UINT_PTR nIDEvent);//OnTimer用
	double fall_cream = 0;//落ちるクリーム
	int cream_count = 0;
	afx_msg void OnBnClickedButton4();

	//以下i-sunが追加した変数
	double lx, ly;               //リニア加速度x,y
	double e4x, e4y, e4z;        //e4各ベクトル
	double dx, dy, dz;           //移動
};

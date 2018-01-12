
// soft_cream_appDlg.h : ヘッダー ファイル
//

#pragma once
#include "afxwin.h"
#include "afxcmn.h"


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
	afx_msg void OnBnClickedRadio2();
	afx_msg void OnBnClickedRadio3();
	afx_msg void OnBnClickedButton3();
	afx_msg void OnBnClickedButton4();
	//以下実験
	unsigned int m_nTimer;//タイマー変数(仮)　使用しない気がする
	afx_msg void OnTimer(UINT_PTR nIDEvent);//OnTimer用
	int hide_cone = 0;//スイッチ
	double fall_cream = 0;//落ちるクリーム　0.5加算で落下表現　Sin関数使う？
	int cream_count = 0;//クリームの数　スイッチ起動から更新カウントを記録
	int cream_number[100];//各クリームの番号として配列
	int cream_color = 0;//クリームの色仮実装用変数　最終的にテクスチャにする

	//以下i-sunが追加した変数
	double lx, ly;               //リニア加速度x,y
	double e4x, e4y, e4z;        //e4各ベクトル
	double dx, dy, dz;           //移動
	double maxCen = 0.0, maxDeg = 0.0;       //傾きと遠心力の最高点
	double saveDeg = 0.0;         //最初の一回目の値を保存する変数
	double deterDeg = 0.0;        //0度から計測した傾き
    int    firstCount = 0;          //一回だけ使うフラグ
	
private:
	CAnimateCtrl m_xcAnimate_Remaining;//アニメーション用　クリーム残量に紐付け
	CAnimateCtrl m_xcAnimate_Result;//結果画像アニメ
	//CAnimateCtrl m_xcAnimate_Result;

public:
	CEdit msgED1;
	CEdit msgED2;

	//1/7　変更箇所
	CEdit msg_ED_G;//最終結果　遠心力
	CEdit msg_ED_D;//　　　　　傾き
	CEdit msg_ED_T;//　　　　　総合評価
	CString result_text;//結果テキスト
	int count_anim=0;//ゲージアニメーションカウント
	double cenF_MAX;//遠心力最大値
	double e4Deg_MAX;//角度最大値

	//1/7　ここまで
};

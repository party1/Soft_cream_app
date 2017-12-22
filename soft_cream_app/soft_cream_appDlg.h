
// soft_cream_appDlg.h : �w�b�_�[ �t�@�C��
//

#pragma once
#include "afxwin.h"


// Csoft_cream_appDlg �_�C�A���O
class Csoft_cream_appDlg : public CDialogEx
{
// �R���X�g���N�V����
public:
	Csoft_cream_appDlg(CWnd* pParent = NULL);	// �W���R���X�g���N�^�[

// �_�C�A���O �f�[�^
	enum { IDD = IDD_SOFT_CREAM_APP_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �T�|�[�g


// ����
protected:
	HICON m_hIcon;

	// �������ꂽ�A���b�Z�[�W���蓖�Ċ֐�
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDestroy();

private:
	CStatic m_glView;
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

	//�ȉ�����
	int hide_cone = 0;//�X�C�b�`
	unsigned int m_nTimer;//�^�C�}�[(��)
	afx_msg void OnTimer(UINT_PTR nIDEvent);//OnTimer�p
	double fall_cream = 0;//������N���[��
	int cream_count = 0;
	afx_msg void OnBnClickedButton4();
};

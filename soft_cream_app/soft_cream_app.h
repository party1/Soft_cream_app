
// soft_cream_app.h : PROJECT_NAME �A�v���P�[�V�����̃��C�� �w�b�_�[ �t�@�C���ł��B
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH �ɑ΂��Ă��̃t�@�C�����C���N���[�h����O�� 'stdafx.h' ���C���N���[�h���Ă�������"
#endif

#include "resource.h"		// ���C�� �V���{��


// Csoft_cream_appApp:
// ���̃N���X�̎����ɂ��ẮAsoft_cream_app.cpp ���Q�Ƃ��Ă��������B
//

class Csoft_cream_appApp : public CWinApp
{
public:
	Csoft_cream_appApp();

// �I�[�o�[���C�h
public:
	virtual BOOL InitInstance();

// ����

	DECLARE_MESSAGE_MAP()
};

extern Csoft_cream_appApp theApp;
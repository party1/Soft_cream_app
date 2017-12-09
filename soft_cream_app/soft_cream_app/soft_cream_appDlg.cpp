
// soft_cream_appDlg.cpp : �����t�@�C��
//

#include "stdafx.h"
#include "soft_cream_app.h"
#include "soft_cream_appDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <math.h>


// �A�v���P�[�V�����̃o�[�W�������Ɏg���� CAboutDlg �_�C�A���O

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// �_�C�A���O �f�[�^
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �T�|�[�g

// ����
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// Csoft_cream_appDlg �_�C�A���O



Csoft_cream_appDlg::Csoft_cream_appDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(Csoft_cream_appDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void Csoft_cream_appDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GLVIEW, m_glView);
}

BEGIN_MESSAGE_MAP(Csoft_cream_appDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


// Csoft_cream_appDlg ���b�Z�[�W �n���h���[

BOOL Csoft_cream_appDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// "�o�[�W�������..." ���j���[���V�X�e�� ���j���[�ɒǉ����܂��B

	// IDM_ABOUTBOX �́A�V�X�e�� �R�}���h�͈͓̔��ɂȂ���΂Ȃ�܂���B
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// ���̃_�C�A���O�̃A�C�R����ݒ肵�܂��B�A�v���P�[�V�����̃��C�� �E�B���h�E���_�C�A���O�łȂ��ꍇ�A
	//  Framework �́A���̐ݒ�������I�ɍs���܂��B
	SetIcon(m_hIcon, TRUE);			// �傫���A�C�R���̐ݒ�
	SetIcon(m_hIcon, FALSE);		// �������A�C�R���̐ݒ�

	// TODO: �������������ɒǉ����܂��B

	//Open_GL������������

	m_pDC = new CClientDC(&m_glView);	// �`��p�̃f�o�C�X�R���e�L�X�g

	if (SetUpPixelFormat(m_pDC->m_hDC) != FALSE){	// �`��\�ł����OpenGL�̏������𑱍s����
		m_GLRC = wglCreateContext(m_pDC->m_hDC);
		wglMakeCurrent(m_pDC->m_hDC, m_GLRC);

		CRect rc;
		m_glView.GetClientRect(&rc);
		GLint width = rc.Width();
		GLint height = rc.Height();
		GLdouble aspect = (GLdouble)width / (GLdouble)height;

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);		// �w�i�͐^�����ɂȂ�
		glViewport(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		//		glOrtho( -aspect, aspect, -1.0, 1.0, -10.0, 10.0);

		gluPerspective(30.0, aspect, 0.1, 100.0);	// �������e���s��
		// ����p30.0�x�A�A�X�y�N�g��͕`��̈�Ɠ����A�`��͈͂ƂȂ�Z�̍ŏ��l��0.1, �ő�l��100.0

		//		gluLookAt( 4.0, 4.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0); 
		gluLookAt(8.0, 25.0, 8.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);	// ���_�̈ʒu�����߂�
		// ���_�ʒu�� (6 6 4)t, �����̐�́@(0 0 0)t, ���\�������x�N�g���� (0 0 1)t

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
	// OpenGL�̏������͂����܂�

	

	return TRUE;  // �t�H�[�J�X���R���g���[���ɐݒ肵���ꍇ�������ATRUE ��Ԃ��܂��B
}

void Csoft_cream_appDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// �_�C�A���O�ɍŏ����{�^����ǉ�����ꍇ�A�A�C�R����`�悷�邽�߂�
//  ���̃R�[�h���K�v�ł��B�h�L�������g/�r���[ ���f�����g�� MFC �A�v���P�[�V�����̏ꍇ�A
//  ����́AFramework �ɂ���Ď����I�ɐݒ肳��܂��B

void Csoft_cream_appDlg::OnPaint()
{

	CPaintDC dc(this); // device context for painting
	// TODO: �����Ƀ��b�Z�[�W �n���h�� �R�[�h��ǉ����܂��B
	// �`�惁�b�Z�[�W�� CStatic::OnPaint() ���Ăяo���Ȃ��ł��������B

	::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//	�Ƃ肠�����A�l�p�`��`�悵�Ă݂�B
	::glPushMatrix();
	::glColor3f(0.0f, 1.0f, 0.0f);
	glColor3d(1.0, 0.0, 0.0);
	glBegin(GL_POLYGON);
	glVertex2d(-0.9, -0.9);
	glVertex2d(0.9, -0.9);
	glVertex2d(0.9, 0.9);
	glVertex2d(-0.9, 0.9);
	glEnd();

	::glPopMatrix();

	::glFinish();
	if (FALSE == ::SwapBuffers(m_pDC->GetSafeHdc())){}


	if (IsIconic())
	{
		CPaintDC dc(this); // �`��̃f�o�C�X �R���e�L�X�g

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// �N���C�A���g�̎l�p�`�̈���̒���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// �A�C�R���̕`��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// ���[�U�[���ŏ��������E�B���h�E���h���b�O���Ă���Ƃ��ɕ\������J�[�\�����擾���邽�߂ɁA
//  �V�X�e�������̊֐����Ăяo���܂��B
HCURSOR Csoft_cream_appDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// For OpenGL on MFC (added from the class view)
// �N���X�r���[����蓮�ɂĒǉ��������\�b�h�֐�
// OpenGL�ɂ������ʃ��[�h�̐ݒ���s���Ă���
// ��{�I�ɂ͂��̊֐��̒��g��ύX���邱�Ƃ͂Ȃ�

BOOL Csoft_cream_appDlg::SetUpPixelFormat(HDC hdc)
{
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL |
		PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		24,
		0, 0, 0, 0, 0, 0,
		0, 0,
		0, 0, 0, 0, 0,
		32,
		0,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	int pf = ChoosePixelFormat(hdc, &pfd);
	if (pf != 0) return SetPixelFormat(hdc, pf, &pfd);
	return false;
}

// For OpenGL on MFC (added from the class view
// �E�B���h�E���j������鎞�ɁA�`��p�̃��\�[�X���J������
// OnDestroy()���\�b�h�̓N���X�r���[��胁�b�Z�[�W�n���h���֐��ǉ��ɂč쐬����
void Csoft_cream_appDlg::OnDestroy()
{
	CDialog::OnDestroy();

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(m_GLRC);
	delete m_pDC;
}


// soft_cream_appDlg.cpp : �����t�@�C��
//x21:����

#include "stdafx.h"
#include "soft_cream_app.h"
#include "soft_cream_appDlg.h"
#include "afxdialogex.h"

//���
#include "stdlib.h"
#include "stdio.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <process.h>
// process.h�̓}���`�X���b�h�`�o�h���g�p���邽�߂ɕK�v
#include <math.h>
// sqrt()���̂��߂ɕK�v
#include <time.h>
// CTime�֘A�֐��̂��߂ɕK�v�i�H�j
#include <mmsystem.h>
// timeGetTime()���g�p���邽��
// �v���W�F�N�g�̍\���v���p�e�B�ɂāA�����J�̒ǉ��ˑ��t�@�C���Ƃ���
// winmm.lib�@��ʓr�w�肵�Ă����K�v������ �v���W�F�N�g���v���p�e�B�������J������

#define WM_RCV (WM_USER+1)

#define MaxComPort	99
#define DefaultPort  4
#define X21_FREQ 100			// 100Hz
#define X21_PERIOD 0.01			// 100Hz Sampling Period
#define DISPLAY_REFRESH	5		// ���l�Ŏ������T���v�������ɉ�ʂ��ĕ`�悷��
#define AXES 3					// �R��
#define PI 3.14159265358979

int RScomport = DefaultPort;	// default COM port = COM4
CString ParamFileName = _T("x21guitar.txt");	// Parameter File
CString DefaultDir = _T("D:\\");
CString CurrentDir = _T("D:\\");
HANDLE	hCOMnd = INVALID_HANDLE_VALUE;

OVERLAPPED rovl, wovl;		// �񓯊��ʐM�p�I�[�o�[���b�v�h�\����
unsigned char wbuf[1024];	// �񓯊����M�p�f�[�^�o�b�t�@

HWND hDlg;		// ���b�Z�[�W�����ɂĎg�p���邽�߂̃_�C�A���O�E�B���h�E���̂̃n���h��
HANDLE serialh;	// ���C�����X��M�X���b�h�̃n���h��
HANDLE packeth;	// �p�����[�^�ݒ�v���g�R������X���b�h�̃n���h��

int rf;						// rf wireless communication switch
int datasize;				// received data count
int etime;					// elapsed time (sec)

#define MAXDATASIZE 1048576	// = 2^20
// 100Hz sampling : 10486sec = 2.9 hours...

#define PACKETSIZE 45		// Preamble(0x65), Seq(1byte), ax, ay, az, gx, gy, gz, temp (2bytes)
// e4x, e4y, e4z, lx, ly, lz, alpha (float : 4bytes), chksum(1byte)
// 1 + 1 + 2x7 + 4x7 + 1 = 2 + 14 + 28 + 1 = 45 bytes

#define PREAMBLE 0x65

int errcnt;		// �p�P�b�g��M�G���[�̌�

// ���[�V�����Z���T�f�[�^�p�o�b�t�@������
// �{����alloc���ɂē��I�Ƀ��������m�ۂ���ׂ������A�����ł̓O���[�o���ϐ��Ƃ��Ă���
int accbuf[AXES][MAXDATASIZE], gyrobuf[AXES][MAXDATASIZE];
int seqbuf[MAXDATASIZE], tempbuf[MAXDATASIZE];

UINT timebuf[MAXDATASIZE];	// Windows�̃V�X�e�����Ԃ��L�^���郁����
double e4buf[AXES][MAXDATASIZE], laccbuf[AXES][MAXDATASIZE], alphabuf[MAXDATASIZE];

// ��ʂւ̃O���t�`��ɂ�����ݒ�p�����[�^
#define INVALID_MODE 0				// �����FY�� = 0�ƂȂ钼�����`�悳���
#define LINEAR_ACCELERATION_MODE 1	// ���j�A�����x
#define ATTITUDE_ANGLE_MODE 2		// �d�͎p���pe4�@�i-1 �` +1�̕����]���\���j
#define ANGULAR_VELOCITY_MODE 3		// �p���x

#define X_AXIS 0
#define Y_AXIS 1
#define Z_AXIS 2


// �O���t�̏c�����x��ݒ肷��p�����[�^
// WIDE_RANGE��I������ƁA�O���t�̏c���͈͂��Z���T�y�ьv�Z���ʁie4, linear acceleration�j��
// �_�C�i�~�b�N�����W�Ɉ�v����B�܂��AMIDDLE -> NARROW�@�ƂȂ�ɏ]���ďc���̊��x���オ��
#define NARROW_RANGE 1
#define MIDDLE_RANGE 2
#define WIDE_RANGE 3

int timescale;
int graph_enable;

static void CloseComPort(void)
{
	if (hCOMnd == INVALID_HANDLE_VALUE)
		return;
	CloseHandle(hCOMnd);
	hCOMnd = INVALID_HANDLE_VALUE;
}

static DWORD OpenComPort(int port)
{
	CString ComPortNum;
	COMMPROP	myComProp;
	DCB	myDCB;
	COMSTAT	myComStat;

	// �񓯊��h�n���[�h�Ȃ̂Ń^�C���A�E�g�͖���
	//	_COMMTIMEOUTS myTimeOut;

	if ((port < 0) || (port > MaxComPort))
		return -1;
	if (port < 10){
		ComPortNum.Format(_T("COM%d"), port);
	}
	else{
		ComPortNum.Format(_T("\\\\.\\COM%d"), port);	// Bill Gates' Magic ...
	}

	ZeroMemory(&rovl, sizeof(rovl));
	ZeroMemory(&wovl, sizeof(wovl));
	rovl.Offset = 0;
	wovl.Offset = 0;
	rovl.OffsetHigh = 0;
	wovl.OffsetHigh = 0;
	rovl.hEvent = NULL;
	wovl.hEvent = NULL;

	hCOMnd = CreateFile((LPCTSTR)ComPortNum, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
	// COM�|�[�g���I�[�o���b�v�h���[�h�i�񓯊��ʐM���[�h�j�ɂăI�[�v�����Ă���

	if (hCOMnd == INVALID_HANDLE_VALUE){
		return -1;
	}

	GetCommProperties(hCOMnd, &myComProp);
	GetCommState(hCOMnd, &myDCB);
	//	if( myComProp.dwSettableBaud & BAUD_128000)
	//		myDCB.BaudRate = CBR_128000;
	//	else
	myDCB.BaudRate = CBR_115200;		// 115.2Kbps���[�h��WindowsAPI�͐������F�����Ȃ�
	//	myDCB.BaudRate = CBR_9600;
	myDCB.fDtrControl = DTR_CONTROL_DISABLE;
	myDCB.Parity = NOPARITY;
	myDCB.ByteSize = 8;
	myDCB.StopBits = ONESTOPBIT;
	myDCB.fDsrSensitivity = FALSE;
	SetCommState(hCOMnd, &myDCB);
	DWORD	d;

	d = myComProp.dwMaxBaud;

	DWORD	myErrorMask;
	char	rbuf[32];
	DWORD	length;

	// �I�[�o�[���b�v�h���[�h�ł́A�^�C���A�E�g�l�͈Ӗ����Ȃ��Ȃ�

	//	GetCommTimeouts( hCOMnd, &myTimeOut);
	//	myTimeOut.ReadTotalTimeoutConstant = 10;	// 10 msec
	//	myTimeOut.ReadIntervalTimeout = 200;	// 200 msec
	//	SetCommTimeouts( hCOMnd, &myTimeOut);
	//	GetCommTimeouts( hCOMnd, &myTimeOut);
	//	ReadTimeOut = (int)myTimeOut.ReadTotalTimeoutConstant;

	ClearCommError(hCOMnd, &myErrorMask, &myComStat);

	if (myComStat.cbInQue > 0){
		int	cnt;
		cnt = (int)myComStat.cbInQue;
		for (int i = 0; i < cnt; i++){
			// Synchronous IO
			//			ReadFile( hCOMnd, rbuf, 1, &length, NULL);		
			//

			// �I�[�o�[���b�v�h���[�h�ŁA�����ʐM�I�Ȃ��Ƃ��s�����߂̃p�b�`�R�[�h
			ReadFile(hCOMnd, rbuf, 1, &length, &rovl);
			while (1){
				if (HasOverlappedIoCompleted(&rovl)) break;
			}
		}
	}

	return d;
}

#define RINGBUFSIZE 1024

// �R�[���o�b�N�֐��ɂ��V���A���ʐM�󋵂̒ʒm����

int rcomp;				// �R�[���o�b�N�֐�����̖߂�l���L�����邽�߂̃O���[�o���ϐ�

VOID CALLBACK FileIOCompletionRoutine(DWORD err, DWORD n, LPOVERLAPPED ovl)
{
	rcomp = n;			// �ǂݍ��񂾃o�C�g�������̂܂܃O���[�o���ϐ��ɕԂ�
}

// �����p�P�b�g����M���邽�߂̃X���b�h

unsigned __stdcall serialchk(VOID * dummy)
{
	DWORD myErrorMask;						// ClearCommError ���g�p���邽�߂̕ϐ�
	COMSTAT	 myComStat;						// ��M�o�b�t�@�̃f�[�^�o�C�g�����擾���邽�߂Ɏg�p����
	unsigned char buf[1024];				// �����p�P�b�g����M���邽�߂̈ꎞ�o�b�t�@

	unsigned char ringbuffer[RINGBUFSIZE];	// �p�P�b�g��͗p�����O�o�b�t�@
	int rpos, wpos, rlen;					// �����O�o�b�t�@�p�|�C���^�iread, write)�A�f�[�^�o�C�g��
	int rpos_tmp;							// �f�[�^��͗pread�ʒu�|�C���^
	int rest;								// ClearCommError�ɂē�����f�[�^�o�C�g�����L�^����

	int seq, expected_seq;					// ���[�V�����Z���T�����M����V�[�P���X�ԍ��i�W�r�b�g�j
	// �ƁA���̊��Ғl�i�G���[���Ȃ���Ύ�M����l�j

	unsigned char packet[PACKETSIZE];		// �p�P�b�g��͗p�o�b�t�@
	int i, chksum;
	int iax, iay, iaz, igx, igy, igz;	// Interger motion data
	int temp;							// Temperature

	float *ftmp;
	UINT timestamp;						// time stamp from the system

	int count = 0;						// Postmessage Counter

	expected_seq = 0;
	rpos = wpos = 0;
	rlen = 0;

	while (rf){
		rcomp = 0;					// FileIOCompletionRoutine���Ԃ���M�o�C�g�����N���A���Ă���
		// �܂��͖����p�P�b�g�̐擪�̂P�o�C�g��ǂݏo���ɍs��
		ReadFileEx(hCOMnd, buf, 1, &rovl, FileIOCompletionRoutine);

		while (1){
			SleepEx(100, TRUE);	// �ő��100�~���b = 0.1�b�̊ԃX���[�v���邪�A���̊Ԃ�
			if (rcomp == 1) break;	// I/O�����R�[���o�b�N�֐������s�����ƃX���[�v����������

			if (!rf){				// �O���v���O��������X���b�h�̒�~���w�����ꂽ���̏���
				CancelIo(hCOMnd);	// ���s�ς݂�ReadFileEx������������Ă���
				break;
			}
		}

		if (!rf) break;				// �O���v���O��������X���b�h�̒�~���w�����ꂽ

		ringbuffer[wpos] = buf[0];
		wpos++;
		wpos = (wpos >= RINGBUFSIZE) ? 0 : wpos;
		rlen++;

		ClearCommError(hCOMnd, &myErrorMask, &myComStat);

		rest = myComStat.cbInQue;	// ��M�o�b�t�@�ɓ����Ă���f�[�^�o�C�g��

		if (rest == 0) continue;		// ���������Ă��Ȃ�����

		rcomp = 0;
		ReadFileEx(hCOMnd, buf, rest, &rovl, FileIOCompletionRoutine);
		SleepEx(16, TRUE);			// Windows�ɂ�����V���A���|�[�g�̕W�����C�e���V�i16msec�j
		if (rcomp != rest){
			CancelIo(hCOMnd);		// ClearCommError�Ŏ擾�����f�[�^�o�C�g���ɖ����Ȃ�
		}							// �f�[�^������M����Ȃ������̂ŁA��ɔ��s����ReadFileEx
		// ���L�����Z�����Ă���i�ق�Fatal error�ɋ߂��j

		i = 0;
		while (rcomp > 0){
			ringbuffer[wpos] = buf[i];	// �����O�o�b�t�@�Ɏ�M�f�[�^��]������
			wpos++;
			wpos = (wpos >= RINGBUFSIZE) ? 0 : wpos;
			rlen++;
			i++;
			rcomp--;
		}

		// ��������p�P�b�g��͂ɓ���

		while (1){					// �L���ȃp�P�b�g�ł�������͂��p������
			while (rlen >= PACKETSIZE){		// ��͊J�n�ɂ͏��Ȃ��Ƃ�45�o�C�g���K�v�ɂȂ�
				if (ringbuffer[rpos] == PREAMBLE) break;
				rpos++;								// �擪��PREAMBLE�ł͂Ȃ�����
				rpos = (rpos >= RINGBUFSIZE) ? 0 : rpos;
				rlen--;								// �����O�o�b�t�@����P�o�C�g�j������
			}
			if (rlen < PACKETSIZE) break;			// �Œ���K�v�ȃ����O�o�b�t�@���g���ʂ�����

			// rlen�ɂ́APREAMBLE���܂߂��S�Ă̎�M�f�[�^�̃o�C�g���������Ă���
			// PREAMBLE��₪��M����Ă���A���㑱�̏��Ȃ��Ƃ�PACKETSIZE-1�o�C�g���������Ă���

			rpos_tmp = rpos;	// �����O�o�b�t�@�����؂��邽�߂̉��|�C���^
			rpos_tmp++;			// packet length �ւ̃|�C���^�ɂȂ�
			rpos_tmp = (rpos_tmp >= RINGBUFSIZE) ? 0 : rpos_tmp;

			chksum = PREAMBLE;

			for (i = 0; i < (PACKETSIZE - 2); i++){
				packet[i] = ringbuffer[rpos_tmp];
				chksum += packet[i];
				rpos_tmp++;
				rpos_tmp = (rpos_tmp >= RINGBUFSIZE) ? 0 : rpos_tmp;
			}

			if ((chksum & 0xff) != ringbuffer[rpos_tmp]){	// �`�F�b�N�T���G���[
				rpos++;										// �擪�̂P�o�C�g = PREAMBLE���������
				rpos = (rpos >= RINGBUFSIZE) ? 0 : rpos;
				rlen--;
				continue;	// ����PREAMBLE��T���ɂ���
			}

			// PREAMBLE�A�`�F�b�N�T���̒l��������PACKETSIZ���̃f�[�^��packet[]�ɓ����Ă���

			timestamp = timeGetTime();	// Windows�̃V�X�e�����Ԃ��擾����

			seq = packet[0];

			if (seq != expected_seq){
				errcnt += (seq + 256 - expected_seq) % 256;	// �p�P�b�g�G���[�����X�V����
			}
			expected_seq = (seq + 1) % 256;

			temp = (packet[13] << 8) + packet[14];
			temp = (temp >= 32768) ? (temp - 65536) : temp;

			iax = (packet[1] << 8) + packet[2];		// 16�r�b�gint�^�ϐ��́A���8�r�b�g�A����8�r�b�g
			iay = (packet[3] << 8) + packet[4];		// �̏��ɑ��M����Ă���
			iaz = (packet[5] << 8) + packet[6];
			igx = (packet[7] << 8) + packet[8];
			igy = (packet[9] << 8) + packet[10];
			igz = (packet[11] << 8) + packet[12];

			iax = (iax >= 32768) ? (iax - 65536) : iax;	// �����Ȃ�16bit�����𕄍��t���ɕϊ����鉉�Z
			iay = (iay >= 32768) ? (iay - 65536) : iay;
			iaz = (iaz >= 32768) ? (iaz - 65536) : iaz;
			igx = (igx >= 32768) ? (igx - 65536) : igx;
			igy = (igy >= 32768) ? (igy - 65536) : igy;
			igz = (igz >= 32768) ? (igz - 65536) : igz;

			accbuf[X_AXIS][datasize] = iax;
			accbuf[Y_AXIS][datasize] = iay;
			accbuf[Z_AXIS][datasize] = iaz;
			gyrobuf[X_AXIS][datasize] = igx;
			gyrobuf[Y_AXIS][datasize] = igy;
			gyrobuf[Z_AXIS][datasize] = igz;
			timebuf[datasize] = timestamp;
			tempbuf[datasize] = temp;
			seqbuf[datasize] = seq;

			ftmp = (float *)&packet[15];			// unsigned char�^�z�񂩂�float�^�ϐ��i4�o�C�g���j�֕ϊ�����
			e4buf[X_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[19];
			e4buf[Y_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[23];
			e4buf[Z_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[27];
			laccbuf[X_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[31];
			laccbuf[Y_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[35];
			laccbuf[Z_AXIS][datasize] = (double)*ftmp;

			ftmp = (float *)&packet[39];
			alphabuf[datasize] = (double)*ftmp;

			datasize++;
			if (datasize >= MAXDATASIZE){
				datasize = (MAXDATASIZE - 1);	// Almost fatal error...
			}

			count++;

			rpos = (rpos + PACKETSIZE) % RINGBUFSIZE;			// �������ǂݏo�����f�[�^�������O�o�b�t�@���珜������
			rlen -= PACKETSIZE;								// �o�b�t�@�̎c��e�ʂ��X�V����

			etime = datasize / X21_FREQ;

			if (count == DISPLAY_REFRESH){			// DISPLAY_REFRESH = 10�ł�0.1�b���ɕ`�悷��ݒ�ƂȂ��Ă���
				PostMessage(hDlg, WM_RCV, (WPARAM)1, NULL);	// �K�v�ɉ����ă��b�Z�[�W�𔭐�������
				count = 0;
			}
		}
	}
	_endthreadex(0);	// �X���b�h�����ł�����
	return 0;
}

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
	, m_xvRadio(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void Csoft_cream_appDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GLVIEW, m_glView);
	DDX_Control(pDX, IDC_GLVIEW2, m_glView2);
	DDX_Control(pDX, IDC_EDIT1, msgED);
	//  DDX_Control(pDX, IDC_PICT0, IDC_PICT0);
	DDX_Radio(pDX, IDC_RADIO1, m_xvRadio);
	DDX_Control(pDX, IDC_ANIMATE1, m_xcAnimate_Remaining);
	//DDX_Control(pDX, IDC_ANIMATE2, m_xcAnimate_Result);
	DDX_Control(pDX, IDC_EDIT2, msgED1);
	DDX_Control(pDX, IDC_EDIT3, msgED2);
	DDX_Control(pDX, IDC_ANIMATE2, m_xcAnimate_Result);
	DDX_Control(pDX, IDC_EDIT4, msg_ED_G);
	DDX_Control(pDX, IDC_EDIT5, msg_ED_D);
	DDX_Control(pDX, IDC_EDIT6, msg_ED_T);
}

BEGIN_MESSAGE_MAP(Csoft_cream_appDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_MESSAGE(WM_RCV, &Csoft_cream_appDlg::OnMessageRCV)
	ON_BN_CLICKED(IDC_BUTTON1, &Csoft_cream_appDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &Csoft_cream_appDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_RADIO1, &Csoft_cream_appDlg::OnBnClickedRadio1)
	ON_BN_CLICKED(IDC_BUTTON3, &Csoft_cream_appDlg::OnBnClickedButton3)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON4, &Csoft_cream_appDlg::OnBnClickedButton4)
	ON_BN_CLICKED(IDC_RADIO2, &Csoft_cream_appDlg::OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_RADIO3, &Csoft_cream_appDlg::OnBnClickedRadio3)
	ON_BN_CLICKED(IDOK, &Csoft_cream_appDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &Csoft_cream_appDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// Csoft_cream_appDlg ���b�Z�[�W �n���h���[

BOOL Csoft_cream_appDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	hDlg = this->m_hWnd;

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

	//�p�P�b�g�֘A�̕ϐ�������
	rf = 0;      
	//	pd = 0;
	datasize = 0;
	errcnt = 0;

	graph_enable = 1;

	CStdioFile pFile;
	CString buf;
	TCHAR pbuf[256], dirbuf[_MAX_PATH];
	int comport, dirlen;

	if (!pFile.Open(ParamFileName, CFile::modeRead)){
		msgED.SetWindowTextW(_T("�ݒ�t�@�C�� x21guitar.txt��������܂���"));
		RScomport = DefaultPort;
		dirlen = GetCurrentDirectory(_MAX_PATH, dirbuf);
		if (dirlen != 0){
			CurrentDir.Format(_T("%s\\"), dirbuf);
		}
	}
	else{
		pFile.ReadString(pbuf, 32);
		comport = ::_ttoi(pbuf);

		dirlen = GetCurrentDirectory(_MAX_PATH, dirbuf);

		if (dirlen != 0){
			CurrentDir.Format(_T("%s\\"), dirbuf);
		}

		CString cpath;
		pFile.ReadString(cpath);

		if (cpath.GetLength() != 0){
			DefaultDir = cpath;
		}

		if ((comport > 0) && (comport <= MaxComPort)){
			RScomport = comport;
			buf.Format(_T("�ʐM�|�[�g�̃|�[�g�ԍ��� %d �ł�"), comport);
			msgED.SetWindowTextW(buf);
		}
		else{
			buf.Format(_T("�ݒ�ł��Ȃ��|�[�g�ԍ��ł� [COM1-COM%d ���L���ł�]"), MaxComPort);
			msgED.SetWindowTextW(buf);
			RScomport = DefaultPort;	// default port number
		}
		pFile.Close();
	}



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
		// TODO: �����Ƀ��b�Z�[�W �n���h�� �R�[�h��ǉ����܂��B
		// �`�惁�b�Z�[�W�� CStatic::OnPaint() ���Ăяo���Ȃ��ł��������B

		//		gluLookAt( 4.0, 4.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0); 
		gluLookAt(20.0, 65.0,20.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);	// ���_�̈ʒu�����߂�
		//8,25,8
		//20,50,20
		//0.70,0
		// ���_�ʒu�� (6 6 4)t, �����̐�́@(0 0 0)t, ���\�������x�N�g���� (0 0 1)t


		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
	// OpenGL�̏������͂����܂�

	//m_nTimer = SetTimer(1, 30, 0);//Timer�Z�b�g�@0.03�b
	//1/5�@�ύX�ӏ�
	//�A�j���[�V����������
	m_xcAnimate_Remaining.Open(L"180-320.avi");
	m_xcAnimate_Remaining.Play(0, -1, 1);
	//SetTimer(2, 1000, 0);//Timer�Z�b�g�@1�b
	SetTimer(3, 30, 0);//Timer�Z�b�g�@0.03�b(�{���͂���Ȃ����Ǔs���ɂ��ݒu)
	//1/5�@�����܂�


	srand((unsigned int)time(NULL));

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
//�R�[����`�ʂ���֐�
void DrawCone(){
	//�R�[���̕`��֐�
	//8�p�`�ŏ�̖ʈ����č\�z
	//�T�C�Y�ύX�\�Ȋ֐�������H
	glColor3f(1.0, 1.0, 0.0);
	glBegin(GL_POLYGON);
	glVertex3d(0.0, 0.0, -8.0);
	glVertex3d(2.0, 0.0, 0.0);
	glVertex3d(1.75, 1.25, 0.0);
	glVertex3d(1.5, 1.5, 0.0);
	glVertex3d(1.25, 1.75, 0.0);
	glVertex3d(0.0, 2.0, 0.0);
	glVertex3d(-1.25, 1.75, 0.0);
	glVertex3d(-1.5, 1.5, 0.0);
	glVertex3d(-1.75, 1.25, 0.0);
	glVertex3d(-2.0, 0.0, 0.0);
	glVertex3d(-1.75, -1.25, 0.0);
	glVertex3d(-1.5, -1.5, 0.0);
	glVertex3d(-1.25, -1.75, 0.0);
	glVertex3d(0.0, -2.0, 0.0);
	glVertex3d(1.25, -1.75, 0.0);
	glVertex3d(1.5, -1.5, 0.0);
	glVertex3d(1.75, -1.25, 0.0);
	glVertex3d(2.0, 0.0, 0.0);
	glEnd();

	glColor3f(1.0, 0.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex3d(2.0, 0.0, 0.0);
	glVertex3d(1.75, 1.25, 0.0);

	glVertex3d(1.5, 1.5, 0.0);
	glVertex3d(1.25, 1.75, 0.0);
	glVertex3d(0.0, 2.0, 0.0);
	glVertex3d(-1.25, 1.75, 0.0);
	glVertex3d(-1.5, 1.5, 0.0);
	glVertex3d(-1.75, 1.25, 0.0);
	glVertex3d(-2.0, 0.0, 0.0);
	glVertex3d(-1.75, -1.25, 0.0);
	glVertex3d(-1.5, -1.5, 0.0);
	glVertex3d(-1.25, -1.75, 0.0);
	glVertex3d(0.0, -2.0, 0.0);
	glVertex3d(1.25, -1.75, 0.0);
	glVertex3d(1.5, -1.5, 0.0);
	glVertex3d(1.75, -1.25, 0.0);
	glVertex3d(2.0, 0.0, 0.0);
	glEnd();
}

void DrawHideCone(){
	//�R�[���B���p�̕ǂ̊֐�
	//4��u���Δ��I�Ȉ͂��ɁI
	//���ŉB���Ă�����̂łǂ����邩
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(6.0, 0.0, 0.0);
	glVertex3d(6.0, 0.0, 4.0);
	glVertex3d(0.0, 0.0, 4.0);
	glEnd();
}

void DrawAxis(){
	//xyz��3�{���������֐�
	glColor3f(1.0, 0.0, 0.0);
	glBegin(GL_LINES);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(18.0, 0.0, 0.0);

	glColor3f(0.0, 1.0, 0.0);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(0.0, 18.0, 0.0);

	glColor3f(0.0, 0.0, 1.0);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(0.0, 0.0, 18.0);
	glEnd();
}

void DrawServer(){
	//�T�[�o�[�p�̕ǂ̊֐�
	//4��u���Δ�
	//�Ȃ񂩕ςɂȂ������ǌ����_�ł͕��u
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(15.0, 0.0, 0.0);
	glVertex3d(15.0, 0.0, 8.0);
	glVertex3d(0.0, 0.0, 8.0);
	glEnd();
}

void DrawFallCream(){
	//������N���[���̊֐�
	//��
	glColor3f(0.0, 1.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex3d(0.0, 0.0, 0.0);
	glVertex3d(1.0, 0.0, 0.0);
	glVertex3d(1.0, 0.0, 4.0);
	glVertex3d(0.0, 0.0, 4.0);
	glEnd();
}

//1/5�@�ύX�ӏ�
void DrawFallCream(int cream_color){
	//������N���[���̊֐�
	//�~��
	double pi = 3.1415;//�~����
	float radius = 1.0;//���a
	float height = 3.0;//����
	int sides = 16;//n�p�`

		glColor3f(0.0, 1.0, 0.0);
		//����
		glNormal3d(0.0, -1.0, 0.0);
		glBegin(GL_POLYGON);
		for (double i = sides; i >= 0; --i) {
			double t = pi * 2 / sides * (double)i;
			glVertex3d(radius * cos(t), radius * sin(t), -height);
		}
		glEnd();


		//����
		if (cream_color == 0){
			glColor3f(0.4, 0.2, 0.2);
		}
		else if (cream_color == 1){
			glColor3f(0.0, 0.5, 0.0);
		}
		else if (cream_color == 2){
			glColor3f(1.0, 0.0, 0.8);
		}
		glBegin(GL_QUAD_STRIP);
		for (double i = 0; i <= sides; i = i + 1){
			double t = i * 2 * pi / sides;
			glNormal3f((GLfloat)cos(t), 0.0, (GLfloat)sin(t));
			glVertex3f((GLfloat)(radius*cos(t)), (GLfloat)(radius*sin(t)), -height);
			glVertex3f((GLfloat)(radius*cos(t)), (GLfloat)(radius*sin(t)), height);
		}
		glEnd();

		//���
		//glColor3f(1.0, 1.0, 1.0);
		glNormal3d(0.0, 1.0, 0.0);
		glBegin(GL_POLYGON);
		for (double i = 0; i < sides; i++) {
			double t = pi * 2 / sides * (double)i;
			glVertex3d(radius * cos(t), radius * sin(t), height);
		}
		glEnd();
}

void DrawFallCream(int cream_color, double fall_cream, int cream_number,int i){
	//�F�@�����ʒu�@�ԍ��@for
	//����4�Ŕ��ʁ@�ԍ��ƌ��݂̗����ʒu�ŕ`��
	//��������O���Ώ��ł���悤��
	if (cream_number == i + 1 && fall_cream >= -30 - 6 * i){
		DrawFallCream(cream_color);
	}
}
//1/5�@�����܂�

void Csoft_cream_appDlg::OnPaint()
{
	/*//1/5�@�ύX�ӏ�
	if (cream_count >= 150){
		//OnPaint���ŃA�j���[�V�����ω�����
		m_xcAnimate_Remaining.Open(L"246.avi");
		m_xcAnimate_Remaining.Play(0, -1, 1);
	}
	//1/5�@�����܂�
	*/
	CPaintDC dc(this); // device context for painting


	//1/7�@�ύX�ӏ�
	//���W����C��


	::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glLineWidth(3.0);


	DrawAxis();//3�{��������

	::glPushMatrix();
	//�N���[���̗�����`��
	if (hide_cone == 1){
		glTranslated(-7.5, 0.0, 15.0); 
		::glPushMatrix();
			glTranslated(7.5, 0.0, 0.0);
			if (fall_cream <= 0){
				glTranslated(0.0, 0.0, fall_cream);
				//1/5�@�ύX�ӏ�
				glRotated(cream_count+2, 0.0, 0.0, 1.0);
				for (int i = 0; i < 15; i++){
					cream_number[i] = i+1;
					glTranslated(0.0, 0.0, 6);
					DrawFallCream(cream_color, fall_cream, cream_number[i],i);
				}
				//1/5�@�����܂�
			}
		::glPopMatrix();
		//1/5�@�ύX�ӏ�
		//�T�[�o�[�̕`��
		DrawServer();
		glTranslated(-3.0, 3.0, 0.0);
		DrawServer();
		glRotated(-90, 0.0, 0.0, 1.0);
		DrawServer();
		glTranslated(0.0, 6.0, 0.0);
		DrawServer();
		//1/5�@�����܂�
	}
	::glPopMatrix();
	
	::glPushMatrix();
	glTranslated(0.0, 0.0, -8.0);

	//4
	//glRotated(90,1,1,0);

	//�R�[���𓮂���
	glPushMatrix();
	glRotated(dx, 1.0, 0.0, 0.0);
	glRotated(dy, 0.0, 1.0, 0.0);
	glRotated(dz, 0.0, 0.0, 1.0);

	glPushMatrix();
	glScaled(1.5, 1.5, 1.5);
	//�R�[���̕\��
	DrawCone();
	glPopMatrix();
	glPopMatrix();

	if (hide_cone == 1){

		//glTranslated(-3.0, 0.0, 10.0);
		//DrawHideCone();
		::glPushMatrix();
		//3���ʂ̃R�[���B��
			glTranslated(-3.0, 3.0, 0.0);
			DrawHideCone();
			glRotated(-90, 0.0, 0.0, 1.0);
			DrawHideCone();
			glTranslated(0.0, 6.0, 0.0);
			DrawHideCone();
		::glPopMatrix();
		//�ꎞ�I��4���ڂ͏o���Ȃ�
		glTranslated(-3.0, -3.0, 0.0);
		DrawHideCone();

	}

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

static double read_data(int mode, int axis, int range, int count)
// x21-e4���o�͂���l�X�Ȑ��l�p�����[�^�𓝈�I�Ɉ������߂̊֐�
// ���̂悤�ȕ��@�ł͏������x���]���ƂȂ���̂́A�R�[�h�̕ێ琫��ǐ���
// ���シ�邽�߁ARapid Prototyping�̎�@�ɂĂ��΂��Ηp�����Ă���
{
	double d;

	if ((axis < X_AXIS) || (axis > Z_AXIS)){
		return 0.0;	// invalid axis
	}

	switch (mode){
	case INVALID_MODE:
		d = 0.0;
		break;
	case LINEAR_ACCELERATION_MODE:
		d = laccbuf[axis][count];
		d *= (1.0 / 8.0);			// �}4G���v���͈͂ł���Ƃ��āA�_�C�i�~�b�N�����W��8G�ƂȂ�
		break;
	case ATTITUDE_ANGLE_MODE:
		d = e4buf[axis][count];
		d *= (1.0 / 2.0);			// e4�i�����]���\���j�ł͒l���-1�`+1�ł���A�_�C�i�~�b�N�����W��2�ƂȂ�
		break;
	case ANGULAR_VELOCITY_MODE:
		d = gyrobuf[axis][count];
		d *= (1.0 / 65536.0);		// �p���x�M����16�r�b�g�����t�������̂��߁A�_�C�i�~�b�N�����W�͖�65536�ł���
		break;					// �����ɂ̓_�C�i�~�b�N�����W��65535�����A���炭�傫�Ȗ��ł͂Ȃ�
	default:
		d = 0.0;
		break;
	}

	switch (range){
	case WIDE_RANGE:
		break;
	case MIDDLE_RANGE:
		d *= 2.0;				// Middle Range�ł͏c�����Q�{�Ɋg�傳���
		break;
	case NARROW_RANGE:
		d *= 5.0;				// Narrow Range�ł͏c���͂T�{�Ɋg�傳���
		break;
	default:
		break;
	}

	return d;
}

LRESULT Csoft_cream_appDlg::OnMessageRCV(WPARAM wParam, LPARAM lParam)
// ���C�����X��M�X���b�h��胁�b�Z�[�W�ɂċ쓮������ʕ`��֐�
{
	int i, j, w, h, x, y, col;
	CString s;
	double dtmp, alpha;

	s.Format(_T("Count = %d Packet Error = %d\n"), etime, errcnt);
	msgED.SetWindowTextW(s);

	
	// �M���x���p�̕\���̈��`�悷��
	CWnd* myPICT0 = GetDlgItem(IDC_PICT0);	// Statud Indicator
	CClientDC myPICTDC0(myPICT0);
	CRect myPICTRECT0;
	myPICT0->GetClientRect(myPICTRECT0);

	alpha = 0.0;
	for (i = 0; i < DISPLAY_REFRESH; i++){
		dtmp = alphabuf[datasize - i];
		alpha = (alpha < dtmp) ? dtmp : alpha;
	}
	// ���̎��_��alpha�ɂ�DISPLAY_REFRESH���ԓ��ɂ�����M���x�W�����̍ő�l������
	col = (int)(alpha*10.0 + 0.5);	// �P�O�i�K�ɗʎq������
	if (col > 8){	// ����0.9�ȏ�@���@e4�ɏ\���ȐM����������
		myPICTDC0.FillSolidRect(myPICTRECT0, RGB(0, 255, 0));			// �O���[��
	}
	else{
		if (col > 3){	// ����0.4�ȏ�0.9�����@���@e4�ɂ��M����������
			myPICTDC0.FillSolidRect(myPICTRECT0, RGB(255, 255, 0));	// �C�G���[
		}
		else{
			// ����0.3�ȉ��łقƂ�ǐM���ł��Ȃ�
			myPICTDC0.FillSolidRect(myPICTRECT0, RGB(255, 0, 0));		// ���b�h
		}
	}

	//--------------------------�f�[�^�̎󂯎��ƕϊ�-----------------------------//
	lx = laccbuf[X_AXIS][datasize-1];
	ly = laccbuf[Y_AXIS][datasize-1];

	e4x = e4buf[X_AXIS][datasize-1];
	e4y = e4buf[Y_AXIS][datasize-1];
	e4z = e4buf[Z_AXIS][datasize-1];

	/*
	//e4�x�N�g����degree�ɕϊ�
	double tmpx = asin(e4x)*(180.0 / PI);
	double tmpy = asin(e4y)*(180.0 / PI);
	double tmpz = asin(e4z)*(180.0 / PI);
	dx = tmpx;
	dy = tmpy;
	dz = tmpz;
	*/
	//--------------------------�����܂�-------------------------------------//

	/*
	if (graph_enable == 0) return TRUE;	// �O���t�`���ON/OFF�����߂�

	// �O���t�`��̈�̃s�N�`���R���g���[�������\�[�XID�ƑΉ��t����
	CWnd* myPICT1 = GetDlgItem(IDC_PICT1);	// CH1 display
	CWnd* myPICT2 = GetDlgItem(IDC_PICT2);	// CH2 display

	// �f�o�C�X�R���e�L�X�g���쐬
	CClientDC myPICTDC1(myPICT1);
	CClientDC myPICTDC2(myPICT2);

	// �`��̈��\���l�p�`�irectangle�j�̍��W
	CRect myPICTRECT1, myPICTRECT2;

	// �O���t��`�悷��
	int gsamples;

	gsamples = timescale*X21_FREQ;		// �`��Ɋ֗^����T���v���̐�
	// �S�ẴT���v����`�悳���邱�ƂɂȂ�

	// CH1 �̃O���t�`��

	myPICT1->GetClientRect(myPICTRECT1);
	w = myPICTRECT1.Width();
	h = myPICTRECT1.Height();

	myPICTDC1.FillSolidRect(myPICTRECT1, RGB(255, 255, 255));

	CPen myPEN01(PS_SOLID, 1, RGB(128, 255, 255));	// �y������F�̓_���ɐݒ�
	CPen* oldPEN01 = myPICTDC1.SelectObject(&myPEN01);
	// �O���t�ɏc�� = 0�̊���i�������j������
	myPICTDC1.MoveTo(0, h >> 1);
	myPICTDC1.LineTo(w, h >> 1);
	myPICTDC1.SelectObject(oldPEN01);

	CPen myPEN1(PS_SOLID, 1, RGB(0, 0, 0));	// �y�������F�ɐݒ�
	CPen* oldPEN1 = myPICTDC1.SelectObject(&myPEN1);

	if (datasize < gsamples){	// ��M�f�[�^�̃T���v��������ʕ�����菭�Ȃ��ꍇ
		dy = read_data(ch1mode, ch1axis, ch1range, 0);
		y = (h >> 1) + (int)(dy*(double)h + 0.5);
		y = clipping(y, h);
		myPICTDC1.MoveTo(0, y);		// �y�����ŏ��̃f�[�^�������ʒu�ֈړ��iMove�j������

		for (i = 1; i < datasize; i++){
			x = i*w / gsamples;
			dy = read_data(ch1mode, ch1axis, ch1range, i);
			y = (h >> 1) + (int)(dy*(double)h + 0.5);
			y = clipping(y, h);
			myPICTDC1.LineTo(x, y);		// �y�����f�[�^�������ʒu�ւƐ��������Ȃ���ړ�������
		}
	}
	else{						// ��ʕ����̎�M�f�[�^�����ɂ���ꍇ
		dy = read_data(ch1mode, ch1axis, ch1range, datasize - gsamples);
		y = (h >> 1) + (int)(dy*(double)h + 0.5);
		y = clipping(y, h);
		myPICTDC1.MoveTo(0, y);

		for (i = (datasize - gsamples + 1), j = 1; i < datasize; i++, j++){
			x = j*w / gsamples;
			dy = read_data(ch1mode, ch1axis, ch1range, i);
			y = (h >> 1) + (int)(dy*(double)h + 0.5);
			y = clipping(y, h);
			myPICTDC1.LineTo(x, y);		// �y�����f�[�^�������ʒu�ւƐ��������Ȃ���ړ�������
		}
	}

	myPICTDC1.SelectObject(oldPEN1);	// �g�����y�������̏�Ԃɖ߂��Ă���

	// CH2 �̃O���t�`��

	myPICT2->GetClientRect(myPICTRECT2);
	w = myPICTRECT2.Width();
	h = myPICTRECT2.Height();

	myPICTDC2.FillSolidRect(myPICTRECT2, RGB(255, 255, 255));

	CPen myPEN02(PS_SOLID, 1, RGB(255, 200, 200));	// �y���𔖐ԐF�̓_���ɐݒ�
	CPen* oldPEN02 = myPICTDC2.SelectObject(&myPEN02);
	// �O���t�ɏc�� = 0�̊���i�������j������
	myPICTDC2.MoveTo(0, h >> 1);
	myPICTDC2.LineTo(w, h >> 1);
	myPICTDC2.SelectObject(oldPEN02);

	CPen myPEN2(PS_SOLID, 1, RGB(0, 0, 0));	// �y�������F�ɐݒ�
	CPen* oldPEN2 = myPICTDC2.SelectObject(&myPEN2);

	if (datasize < gsamples){	// ��M�f�[�^�̃T���v��������ʕ�����菭�Ȃ��ꍇ
		dy = read_data(ch2mode, ch2axis, ch2range, 0);
		y = (h >> 1) + (int)(dy*(double)h + 0.5);
		y = clipping(y, h);
		myPICTDC2.MoveTo(0, y);		// �y�����ŏ��̃f�[�^�������ʒu�ֈړ��iMove�j������

		for (i = 1; i < datasize; i++){
			x = i*w / gsamples;
			dy = read_data(ch2mode, ch2axis, ch2range, i);
			y = (h >> 1) + (int)(dy*(double)h + 0.5);
			y = clipping(y, h);
			myPICTDC2.LineTo(x, y);		// �y�����f�[�^�������ʒu�ւƐ��������Ȃ���ړ�������
		}
	}
	else{						// ��ʕ����̎�M�f�[�^�����ɂ���ꍇ
		dy = read_data(ch2mode, ch2axis, ch2range, datasize - gsamples);
		y = (h >> 1) + (int)(dy*(double)h + 0.5);
		y = clipping(y, h);
		myPICTDC2.MoveTo(0, y);

		for (i = (datasize - gsamples + 1), j = 1; i < datasize; i++, j++){
			x = j*w / gsamples;
			dy = read_data(ch2mode, ch2axis, ch2range, i);
			y = (h >> 1) + (int)(dy*(double)h + 0.5);
			y = clipping(y, h);
			myPICTDC2.LineTo(x, y);		// �y�����f�[�^�������ʒu�ւƐ��������Ȃ���ړ�������
		}
	}

	myPICTDC2.SelectObject(oldPEN2);	// �g�����y�������̏�Ԃɖ߂��Ă���
	*/


#if 0
	// �������r�b�g�}�b�v�\������ۂ̃T���v���R�[�h

	CString Rating;

	if (lx > ly){
		if (lx > lz){	// lx is the largest
			Rating = _T("X");
		}
		else{			// lz is the largest
			Rating = _T("Z");
		}
	}
	else{
		if (ly > lz){	// ly is the largest
			Rating = _T("Y");
		}
		else{			// lz is the largest
			Rating = _T("Z");
		}
	}

	CFont myFont, *oldFont;

	myFont.CreateFont(240, 160, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, _T("Arial"));

	myPICTR->GetClientRect(myPICTRECTR);
	myPICTDCR.FillSolidRect(myPICTRECTR, RGB(255, 255, 255));
	oldFont = myPICTDCR.SelectObject(&myFont);
	myPICTDCR.TextOutW(40, 20, Rating);
	myPICTDCR.SelectObject(oldFont);

	CFont myFont2;

	myFont2.CreateFont(40, 20, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, _T("Arial"));

	CString AlphaValue;

	AlphaValue.Format(_T("%7.5f"), alpha);

	myPICTA->GetClientRect(myPICTRECTA);
	myPICTDCA.FillSolidRect(myPICTRECTA, RGB(255, 255, 255));
	oldFont = myPICTDCA.SelectObject(&myFont2);
	myPICTDCA.TextOutW(0, 0, AlphaValue);
	myPICTDCA.SelectObject(oldFont);

#endif

	return TRUE;
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


void Csoft_cream_appDlg::OnBnClickedButton1()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	// Connect
	// ���C�����X��M�X���b�h���N������
	
	DWORD d;
	unsigned int tid;

	if (rf){
		msgED.SetWindowTextW(_T("���łɌv�����s���Ă��܂�"));
		return;
	}

	d = OpenComPort(RScomport);

	if (d < 0){
		msgED.SetWindowTextW(_T("�����p�̒ʐM�|�[�g���������ł��܂���ł���"));
		Invalidate();
		UpdateWindow();
		return;
	}

	if (datasize == 0){		// �t�@�C���ɏ����o���ׂ��f�[�^���c���Ă��Ȃ�
		errcnt = 0;			// ������datasize���[���łȂ���΁A�v������U���f���ꂽ���
		etime = 0;			// �Ăю��s����Ă���ƌ��Ȃ��Ă���
	}

	rf = 1;

	serialh = (HANDLE)_beginthreadex(NULL, 0, serialchk, NULL, 0, &tid);

	if (serialh == NULL){
		rf = 0;
		CloseComPort();
		msgED.SetWindowTextW(_T("���[�V�����̌v�������s�ł��܂���ł���"));
		return;
	}

	msgED.SetWindowTextW(_T("�v�����J�n���܂���"));


	// ���[�V�����Z���T�f�o�C�X�ւ̃p�����[�^���M���s���ۂ̊g���p�R�[�h
#if 0
	pd = 1;

	packeth = (HANDLE)_beginthreadex(NULL, 0, device_init, NULL, 0, &tid);
	// �f�o�C�X�p�����[�^�ݒ�p�̃X���b�h���N������

	if (packeth == NULL){
		rf = 0;
		pd = 0;
		CloseComPort();
		msgED.SetWindowTextW(_T("���[�V�����̌v���͎��s�ł��܂���ł���[2]"));
		return;
	}
#endif
}




void Csoft_cream_appDlg::OnBnClickedButton2()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	// OFF
	// ���C�����X��M�X���b�h���~������
	// �t�@�C���ւ̃f�[�^�����o�����s���ۂɂ����C�����X��M�X���b�h�͒�~���邽�߁A
	// ���̃��\�b�h�͈��ً̋}��~�p�ƂȂ��Ă���

	if (rf == 0){
		msgED.SetWindowTextW(_T("�v���͍s���Ă��܂���ł���"));
		return;
	}
	else{
		rf = 0;

		DWORD dwExitCode;
		while (1){
			GetExitCodeThread(serialh, &dwExitCode);
			if (dwExitCode != STILL_ACTIVE) break;
		}
		CloseHandle(serialh);
		serialh = NULL;
		CloseComPort();
		msgED.SetWindowTextW(_T("�v�����~���܂���"));
	}
}





//1/5�@�ύX�ӏ��@�قڑS��
void Csoft_cream_appDlg::OnBnClickedButton3()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	//�v���g�^�C�v���[�h�쓮�{�^��
	//�B���ǂ�\�����ĕ`����J�n����
	hide_cone = 1;
	SetTimer(1, 30, 0);//Timer�Z�b�g�@0.03�b
	//�{�^���������ꍇ�̏���
	m_xcAnimate_Remaining.Open(L"180-320.avi");
	m_xcAnimate_Remaining.Play(0, -1, 1);
	GetDlgItem(IDC_GLVIEW)->ShowWindow(SW_SHOW);
	OnPaint();
}

//12/22 �ǉ��� ���S�͂��v�Z���ĕԂ��֐�
double centrifugal_force_cal(double lx, double ly, double e4z)
{
	double cForce = 0;
	// ���j�A�����xx,y�̕������a
	cForce = abs(sqrt((lx*lx) + (ly*ly))) + abs(sqrt(1 - (e4z*e4z)));

	return cForce;
}

//e4�ɂăf�o�C�X�̌X����degree�ŕԂ��֐�
double e4z_conversion_degree(double e4z)
{
	double deg = 0;
	//e4z�̌X����degree�ɒ���
	deg = asin(e4z) * 180 / PI;

	return deg;
}


void Csoft_cream_appDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: �����Ƀ��b�Z�[�W �n���h���[ �R�[�h��ǉ����邩�A����̏������Ăяo���܂��B
	double tmpx = asin(e4x)*(180.0 / PI);
	double tmpy = asin(e4y)*(180.0 / PI);
	double tmpz = asin(e4z)*(180.0 / PI);
	dx = -tmpx;
	dy = -tmpy;
	dz = -tmpz;
	
	double cenF = centrifugal_force_cal(lx, ly, e4z);
	double e4Deg = e4z_conversion_degree(e4z);

	//���Ɉړ�

	CString cf, ed;

	cf.Format(_T("%f"), cenF);
	ed.Format(_T("%f"), e4Deg);
	msgED1.SetWindowTextW(cf);
	msgED2.SetWindowTextW(ed);

	//setTimer�̉񐔖��ɌĂяo��
	//ID1��0.03�b��
	//ID2��1�b��

	if (hide_cone == 1){
		if (nIDEvent == 1){
			//�����𖞂����Ă���ꍇ
			//�A�C�X�N���[�������A���s�J�E���g���Z�A�`��
			//�C�`�S�̏ꍇ�̂ݑ��x�ω�
			if (cream_color == 2){//�C�`�S
				if (cream_count <= 400){//400�܂�
					if (((int)cream_count % 50) == 0){//50����
						if ((rand() % 3 + 1) == 1){//�����_��
							fall_cream_ch = 0.2;
							cream_count_ch = 0.8;
						}
						else if ((rand() % 3 + 1) == 2){
							fall_cream_ch = 0.7;
							cream_count_ch = 2.8;
						}
						else{
							fall_cream_ch = 0.5;
							cream_count_ch = 2;
						}
						//�ύX���ꂽ���̂�`��
						fall_cream -= fall_cream_ch;
						cream_count += cream_count_ch;
					}
					else{
						//50���łȂ��ꍇ�͐ݒ肳�ꂽ�l�ŗ���
						fall_cream -= fall_cream_ch;
						cream_count += cream_count_ch;
					}
				}
				else{
					if (cream_count <= 500){
						//400�ȍ~�͕ύX�Ȃ�
						fall_cream -= fall_cream_ch;
						cream_count += cream_count_ch;
					}
					else{
						//500�ȍ~�͓�����
						fall_cream -= 0.5;
						cream_count += 2;
					}
					
				}
			}
			else{
				fall_cream -= 0.5;
				cream_count += 2;
			}
			OnPaint();

			//���S�͂ƌX���̍ō��_��ۑ����Ă���
			//�������Ɉړ�
			if (cream_count <= 600&&maxCen < cenF){
				maxCen = cenF;

			}
			if (cream_count <= 600 && maxDeg < e4Deg){
				maxDeg = e4Deg;

			}
			//1/11�@�ύX�ӏ�

			if (cream_count <= 500){//500�ȉ���50�̔{��
				//�N���[���J�E���g�ŃA�j���[�V�����ω��e�X�g
				//double�^�ɂ��������Ō��̕��@���g���Ȃ��Ȃ����̂Ŏd���Ȃ���
				if (count_anim == 0&&cream_count <= 50){
					m_xcAnimate_Remaining.Open(L"0m.avi");
					m_xcAnimate_Remaining.Play(0, -1, 1);
				}
				else if (count_anim == 1 && cream_count <= 100){
					m_xcAnimate_Remaining.Open(L"1m.avi");
					m_xcAnimate_Remaining.Play(0, -1, 1);
				}
				else{

				}

				count_anim++;
			}

			//1/11�@�����܂�

			if (cream_count > 600 || maxCen >= 2.0 || maxDeg >= 10){//1/7�@�ύX�ӏ� �Q�[���I�[�o�[���O���z�u
				GetDlgItem(IDC_GLVIEW)->ShowWindow(SW_HIDE);
				GetDlgItem(IDC_ANIMATE2)->ShowWindow(SW_SHOW);
				//1/7�@�ύX�ӏ�
				//���ʉ�ʂ̕\��
				GetDlgItem(IDC_STATIC_1)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_STATIC_2)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_STATIC_3)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_STATIC_4)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_STATIC_5)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_EDIT4)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_EDIT5)->ShowWindow(SW_SHOW);
				GetDlgItem(IDC_EDIT6)->ShowWindow(SW_SHOW);
				//1/7�@�����܂�
				//m_xcAnimate_Result.Open(L"250-400 (2).avi");
				//m_xcAnimate_Result.Play(0, -1, 1);

				//1/11�@�ύX�ӏ�
				//���ʔ���

				//���s���ɂ�����ő�̒l��\���@�Ƃ肠�������z�u
				//�ǂ��ő�l�����Ă�̂��m��Ȃ��̂œK����
				//double cenF_MAX = 0.1;//���w�b�_�[����
				//double e4Deg_MAX = 30;//����

				maxCen = 2.0;//��
				maxDeg = 10;//����

				CString cf_MAX, ed_MAX;

				cf_MAX.Format(_T("%f"), maxCen);
				ed_MAX.Format(_T("%f"), maxDeg);
				msg_ED_G.SetWindowTextW(cf_MAX);
				msg_ED_D.SetWindowTextW(ed_MAX);

				//���ʔ��艼�z�u
				//���͂ǂ����邩�͌��ʉ摜�ƍl����H

				if (maxCen >= 2.0 || maxDeg >= 10){
					result_text = _T("�c�O�Ȃ������܂���\r\n");
					m_xcAnimate_Result.Open(L"���s2.avi");
					m_xcAnimate_Result.Play(0, -1, 1);
					if (maxCen < 2.0){
						result_text += _T("�X���������݂����ł�\r\n");
					}
					else{
						result_text += _T("�����񂵂����̂悤�ł�\r\n");
					}
				}
				else if (maxCen < 2.0 || maxDeg < 10){
					result_text = _T("�悭�ł��܂���\r\n");
					m_xcAnimate_Result.Open(L"250-400 (2).avi");
					m_xcAnimate_Result.Play(0, -1, 1);
				}

				msg_ED_T.SetWindowTextW(result_text);
				
				KillTimer(1);
				//1/11�@�����܂�
			}
		}
	}
	CDialogEx::OnTimer(nIDEvent);
}


void Csoft_cream_appDlg::OnBnClickedButton4()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	//�S����OFF�@�܂胊�Z�b�g
	hide_cone = 0;
	fall_cream = 0;
	cream_count = 0;
	for(int i = 0; i < 100; i++){
		cream_number[i]=0;
	}
	GetDlgItem(IDC_GLVIEW)->ShowWindow(SW_SHOW);
	GetDlgItem(IDC_ANIMATE2)->ShowWindow(SW_HIDE);
	//1/7�@�ύX�ӏ�
	//���ʉ�ʂ̕\��
	GetDlgItem(IDC_STATIC_1)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_2)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_3)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_4)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_STATIC_5)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT4)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT5)->ShowWindow(SW_HIDE);
	GetDlgItem(IDC_EDIT6)->ShowWindow(SW_HIDE);
	count_anim = 0;
	//1/7�@�����܂�

	//1/11�@�ύX�ӏ�
	//�ǉ������ϐ������Z�b�g
	maxCen = 0;
	maxDeg = 0;
	fall_cream_ch = 0;
	cream_count_ch = 0;
	KillTimer(1);
	OnPaint();

}

void Csoft_cream_appDlg::OnBnClickedRadio1()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	//���W�I�{�^��3��
	//�ꍇ�ɂ���Ă͕��ʂɃ{�^��3�ɂȂ邪�����_�ł͂��̕������̗p
	//�N�����Ă��Ȃ��ꍇ�ɐF��I��
	if (hide_cone == 0){
		cream_color = 0;
	}
	else{
		
	}
}

void Csoft_cream_appDlg::OnBnClickedRadio2()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	if (hide_cone == 0){
		cream_color = 1;
	}
	else{
		
	}
}


void Csoft_cream_appDlg::OnBnClickedRadio3()
{

	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	if (hide_cone == 0){
		cream_color = 2;
	}
	else{
	}
}



//1/11�@�ύX�ӏ�
//�I������Timer��~��ǉ�
void Csoft_cream_appDlg::OnBnClickedOk()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	KillTimer(1);
	KillTimer(3);
	CDialogEx::OnOK();
}


void Csoft_cream_appDlg::OnBnClickedCancel()
{
	// TODO: �����ɃR���g���[���ʒm�n���h���[ �R�[�h��ǉ����܂��B
	KillTimer(1);
	KillTimer(3);
	CDialogEx::OnCancel();
}
//1/11�@�����܂�